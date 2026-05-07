#include "CameraListenerSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/Audio/IAudioAPI.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

CameraListenerSystem::CameraListenerSystem() : SystemBase("CameraListenerSystem") {}

void CameraListenerSystem::Update(float dt) {
    if (!_world) return;

    // カメラとオーディオの両方が存在するかチェック
    if (!_world->HasResource<Camera*>() || !_world->HasResource<std::shared_ptr<IAudioAPI>>()) return;

    Camera* mainCamera = _world->GetResource<Camera*>();
    auto& audioAPI = _world->GetResource<std::shared_ptr<IAudioAPI>>();

    if (!mainCamera || !audioAPI) return;

    // カメラの情報を取得
    Vector3 pos = mainCamera->GetEye();
    Vector3 lookAt = mainCamera->GetFocus();
    Vector3 up = mainCamera->GetUp();

    // 視線ベクトル（Forward）を計算
    Vector3 forward = lookAt - pos;
    forward.Normalize();

    // FMODにリスナー（耳）の情報を送信
    audioAPI->SetListenerPosition(
        { pos.x, pos.y, pos.z },
        { forward.x, forward.y, forward.z },
        { up.x, up.y, up.z }
    );
}

// カメラの位置が確定した後（RenderStageの最初の方）で実行する
REGISTER_RENDER_SYSTEM(CameraListenerSystem, Priority::RenderStage::R02_Environment);