#include "CameraBrainSystem.h"

// 実装で必要なヘッダ群
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Core/Camera.h"
#include <algorithm>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;
using namespace DirectX;

CameraBrainSystem::CameraBrainSystem() : SystemBase("CameraBrainSystem")
{
    // 全てのVirtualCameraコンポーネントを持つエンティティを対象にする
    _filter = CCL::ECS::ArchetypeHelper::Generate<VirtualCamera>();
}

std::vector<CCL::ECS::TypeID> CameraBrainSystem::GetReadTypes() const
{
    return {CCL::ECS::TypeInfo<VirtualCamera>::ID()};
}

void CameraBrainSystem::Update(float dt)
{
    // Worldからカメラリソースを取得
    if (!_world->HasResource<Camera *>()) return;
    Camera *mainCamera = _world->GetResource<Camera *>();
    if (!mainCamera) return;

    // -------------------------------------------------------
    // 1. 最も優先度が高い vCam を探す
    // -------------------------------------------------------
    EntityID       highestCamID = InvalidEntityID;
    int            maxPriority  = -99999;
    VirtualCamera *highestVCam  = nullptr;

    for (auto *chunk : _targetChunks) {
        auto  count = chunk->GetEntityCount();
        auto *vCams = chunk->GetComponentArray<VirtualCamera>();
        auto *ids   = chunk->GetEntityIDs();

        for (size_t i = 0; i < count; ++i) {
            if (chunk->IsEntityDestroyed(i)) continue;

            if (vCams[i].priority > maxPriority) {
                maxPriority  = vCams[i].priority;
                highestCamID = ids[i];
                highestVCam  = &vCams[i];
            }
        }
    }

    if (highestCamID == InvalidEntityID || highestVCam == nullptr) return;

    // -------------------------------------------------------
    // 2. カメラ切り替え検知 & ブレンド開始
    // -------------------------------------------------------
    if (highestCamID != _currentCamID) {
        // 初回起動時なら即切り替え
        if (_currentCamID == InvalidEntityID) {
            _currentCamID = highestCamID;
            _blendTimer   = highestVCam->blendTime + 1.0f; // スキップ
        }
        else {
            // 遷移開始
            _prevCamID    = _currentCamID;
            _currentCamID = highestCamID;
            _blendTimer   = 0.0f;

            // 固定値ではなく、新しいカメラの設定値を使う
            _blendDuration = highestVCam->blendTime;
        }
    }

    // -------------------------------------------------------
    // 3. ブレンド計算
    // -------------------------------------------------------
    XMVECTOR finalPos;
    XMVECTOR finalLook;
    float    finalFov;
    float    finalNear;
    float    finalFar;

    // 次のカメラ（ターゲット）の情報
    XMVECTOR targetPos  = XMLoadFloat3(&highestVCam->resultPos);
    XMVECTOR targetLook = XMLoadFloat3(&highestVCam->resultLookAt);
    float    targetFov  = highestVCam->fov;
    float    targetNear = highestVCam->nearClip;
    float    targetFar  = highestVCam->farClip;

    bool isBlending = (_blendTimer < _blendDuration);

    if (isBlending) {
        _blendTimer += dt;
        float t = _blendTimer / _blendDuration;
        t       = (std::min)(t, 1.0f);
        t       = t * t * (3.0f - 2.0f * t); // Ease-in-out

        VirtualCamera *prevVCam = _world->GetComponent<VirtualCamera>(_prevCamID);

        XMVECTOR srcPos, srcLook;
        float    srcFov, srcNear, srcFar;

        if (prevVCam) {
            srcPos  = XMLoadFloat3(&prevVCam->resultPos);
            srcLook = XMLoadFloat3(&prevVCam->resultLookAt);
            srcFov  = prevVCam->fov;
            srcNear = prevVCam->nearClip;
            srcFar  = prevVCam->farClip;
        }
        else {
            // バックアップを使用
            srcPos  = XMLoadFloat3(&_lastPos);
            srcLook = XMLoadFloat3(&_lastLook);
            srcFov  = _lastFov;
            srcNear = _lastNear;
            srcFar  = _lastFar;
        }

        finalPos  = XMVectorLerp(srcPos, targetPos, t);
        finalLook = XMVectorLerp(srcLook, targetLook, t);
        finalFov  = srcFov + (targetFov - srcFov) * t;
        finalNear = srcNear + (targetNear - srcNear) * t;
        finalFar  = srcFar + (targetFar - srcFar) * t;

        if (t >= 1.0f) {
            _prevCamID = InvalidEntityID;
        }
    }
    else {
        // ブレンドなし
        finalPos  = targetPos;
        finalLook = targetLook;
        finalFov  = targetFov;
        finalNear = targetNear;
        finalFar  = targetFar;
    }

    // -------------------------------------------------------
    // 4. メインカメラへ反映
    // -------------------------------------------------------
    XMFLOAT3 fPos, fLook;
    XMStoreFloat3(&fPos, finalPos);
    XMStoreFloat3(&fLook, finalLook);

    mainCamera->SetLookAt(fPos, fLook, {0, 1, 0});

    // 安全対策 (Safety Clamp)
    float safeNear = (std::max)(finalNear, 0.01f);
    float safeFar  = (std::max)(finalFar, safeNear + 0.1f);

    float aspect = mainCamera->GetAspect();

    // セット
    mainCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(finalFov), aspect, safeNear, safeFar);

    // 次フレームのためにバックアップ
    _lastPos  = fPos;
    _lastLook = fLook;
    _lastFov  = finalFov;
    _lastNear = safeNear;
    _lastFar  = safeFar;
}

// ==========================================
// マクロは必ず .cpp の末尾に1回だけ書く
// ==========================================
REGISTER_RENDER_SYSTEM(CameraBrainSystem, Priority::RenderStage::R01_Camera);