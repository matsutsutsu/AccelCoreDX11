#include "JoltDebugDrawSystem.h"
#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace {
    // クォータニオンからオイラー角(ラジアン)への変換関数 (ShapeRendererのDrawBox用)
    DirectX::XMFLOAT3 QuatToEulerRadian(const DirectX::XMFLOAT4 &q) {
        float pitch = std::asin(std::clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f));
        float yaw   = std::atan2(2.0f * (q.w * q.y + q.z * q.x), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        float roll  = std::atan2(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));
        return {pitch, yaw, roll};
    }
    
    // デバッグ用の色（薄い緑色など、お好みで変更してください）
    const DirectX::XMFLOAT4 COLLIDER_COLOR = {0.2f, 1.0f, 0.2f, 1.0f}; 
}

// ===================================================================================
// 1. BoxCollider の描画
// ===================================================================================
void JoltBoxDebugDrawSystem::Update(float dt)
{
    if (!isDebugVisible) return;
    if (!_world->HasResource<ShapeRenderer*>()) return;
    auto* renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    ForEach([&](const TransformComponent& trans, const JoltBoxColliderComponent& box) {
        // ====================================================================
        // ★ 修正: ローカルではなく、計算済みのワールド座標と回転を取得
        // ====================================================================
        DirectX::XMFLOAT3 worldPos = trans.GetWorldPosition();
        DirectX::XMFLOAT4 worldRotQuat = trans.GetWorldRotation();

        // 1. 本体(Entity)のワールド行列 (スケールは物理挙動を壊すため排除)
        XMMATRIX worldRot = XMMatrixRotationQuaternion(XMLoadFloat4(&worldRotQuat));
        XMMATRIX worldTrans = XMMatrixTranslation(worldPos.x, worldPos.y, worldPos.z);
        XMMATRIX worldMat = worldRot * worldTrans;

        // 2. コライダー単体のローカルズレ行列
        XMMATRIX localRot = XMMatrixRotationRollPitchYaw(XMConvertToRadians(box.localRotationEuler.x), XMConvertToRadians(box.localRotationEuler.y), XMConvertToRadians(box.localRotationEuler.z));
        XMMATRIX localTrans = XMMatrixTranslation(box.localOffset.x, box.localOffset.y, box.localOffset.z);
        XMMATRIX localMat = localRot * localTrans;

        // 3. 結合 (ローカルのズレを適用してから、ワールドに配置する)
        XMMATRIX finalMat = localMat * worldMat;

        // 最終的な座標と回転を抽出して描画
        XMVECTOR outScale, outRotQuatV, outTrans;
        XMMatrixDecompose(&outScale, &outRotQuatV, &outTrans, finalMat);

        XMFLOAT3 finalPos, finalEuler;
        XMStoreFloat3(&finalPos, outTrans);

        // ShapeRendererのDrawBoxはオイラー角(Radian)を要求するため変換
        XMFLOAT4 q; XMStoreFloat4(&q, outRotQuatV);
        finalEuler = QuatToEulerRadian(q);

        DirectX::XMFLOAT3 size = box.halfExtent;

        renderer->DrawBox(finalPos, finalEuler, size, COLLIDER_COLOR);
        });
}

// ===================================================================================
// 2. SphereCollider の描画
// ===================================================================================
void JoltSphereDebugDrawSystem::Update(float dt)
{
    if (!isDebugVisible) return;
    if (!_world->HasResource<ShapeRenderer*>()) return;
    auto* renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    ForEach([&](const TransformComponent& trans, const JoltSphereColliderComponent& sphere) {
        // ★ ワールド座標と回転を取得
        DirectX::XMFLOAT3 worldPos = trans.GetWorldPosition();
        DirectX::XMFLOAT4 worldRotQuat = trans.GetWorldRotation();

        // 本体ワールド行列
        XMMATRIX worldRot = XMMatrixRotationQuaternion(XMLoadFloat4(&worldRotQuat));
        XMMATRIX worldTrans = XMMatrixTranslation(worldPos.x, worldPos.y, worldPos.z);
        XMMATRIX worldMat = worldRot * worldTrans;

        // ローカル位置ズレ行列
        XMMATRIX localTrans = XMMatrixTranslation(sphere.localOffset.x, sphere.localOffset.y, sphere.localOffset.z);

        XMMATRIX finalMat = localTrans * worldMat;

        XMVECTOR outScale, outRot, outTrans;
        XMMatrixDecompose(&outScale, &outRot, &outTrans, finalMat);
        XMFLOAT3 finalPos;
        XMStoreFloat3(&finalPos, outTrans);

        renderer->DrawSphere(finalPos, sphere.radius, COLLIDER_COLOR);
        });
}

// ===================================================================================
// 3. CapsuleCollider の描画
// ===================================================================================
void JoltCapsuleDebugDrawSystem::Update(float dt)
{
    if (!isDebugVisible) return;
    if (!_world->HasResource<ShapeRenderer*>()) return;
    auto* renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    ForEach([&](const TransformComponent& trans, const JoltCapsuleColliderComponent& capsule) {
        // ★ ワールド座標と回転を取得
        DirectX::XMFLOAT3 worldPos = trans.GetWorldPosition();
        DirectX::XMFLOAT4 worldRotQuat = trans.GetWorldRotation();

        // 本体ワールド行列
        XMMATRIX worldRot = XMMatrixRotationQuaternion(XMLoadFloat4(&worldRotQuat));
        XMMATRIX worldTrans = XMMatrixTranslation(worldPos.x, worldPos.y, worldPos.z);
        XMMATRIX worldMat = worldRot * worldTrans;

        // ローカルズレ行列
        XMMATRIX localRot = XMMatrixRotationRollPitchYaw(XMConvertToRadians(capsule.localRotationEuler.x), XMConvertToRadians(capsule.localRotationEuler.y), XMConvertToRadians(capsule.localRotationEuler.z));
        XMMATRIX localTrans = XMMatrixTranslation(capsule.localOffset.x, capsule.localOffset.y, capsule.localOffset.z);
        XMMATRIX localMat = localRot * localTrans;

        // 結合
        XMMATRIX finalMat = localMat * worldMat;

        XMFLOAT4X4 mat;
        XMStoreFloat4x4(&mat, finalMat);

        renderer->DrawCapsule(mat, capsule.radius, capsule.halfHeight * 2.0f, COLLIDER_COLOR);
        });
}


// ===================================================================================
// 4. MeshCollider の描画
// ===================================================================================
void JoltMeshDebugDrawSystem::Update(float dt)
{
    if (!isDebugVisible) return;
    if (!_world->HasResource<ShapeRenderer*>()) return;
    auto* renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    // Transform, Meshタグ, Modelの3つが揃っているエンティティだけを描画
    ForEach([&](const TransformComponent& trans, 
                const JoltMeshColliderComponent& meshCol, 
                const ModelComponent& modelComp) {
                    
        // タグがOFF、またはモデルがロードされていない場合はスキップ
        if (!meshCol.isEnabled || !modelComp.GetModel()) return;

        const ModelResource* res = modelComp.GetModel()->GetResource();
        if (!res) return;

        // ====================================================================
        // ★ 究極の最適化: TransformComponent はすでに「完全なワールド行列(worldMatrix)」
        // を持っているため、個別の成分を掛け合わせる必要すらありません。
        // ====================================================================
        XMMATRIX worldMat = XMLoadFloat4x4(&trans.worldMatrix);

        // 全メッシュの全ポリゴンをループして三角形を描画
        for (const auto& mesh : res->GetMeshes()) {
            
            // indices は 3つで1つの三角形を構成する
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                uint32_t i0 = mesh.indices[i + 0];
                uint32_t i1 = mesh.indices[i + 1];
                uint32_t i2 = mesh.indices[i + 2];

                // 頂点のローカル座標を取得
                XMVECTOR v0 = XMLoadFloat3(&mesh.vertices[i0].position);
                XMVECTOR v1 = XMLoadFloat3(&mesh.vertices[i1].position);
                XMVECTOR v2 = XMLoadFloat3(&mesh.vertices[i2].position);

                // ワールド空間（実際の画面上の位置）に変換
                v0 = XMVector3Transform(v0, worldMat);
                v1 = XMVector3Transform(v1, worldMat);
                v2 = XMVector3Transform(v2, worldMat);

                XMFLOAT3 p0, p1, p2;
                XMStoreFloat3(&p0, v0);
                XMStoreFloat3(&p1, v1);
                XMStoreFloat3(&p2, v2);

                // 3点を結んで三角形（ワイヤーフレーム）を描画
                renderer->DrawTriangle(p0, p1, p2, COLLIDER_COLOR);
            }
        }
    });
}

// ===================================================================================
// システムの登録
// ===================================================================================
// ※ OnDrawDebug はシステムの実行順序に依存しないため、適当なLogicフェーズに登録しておくだけで機能します。
REGISTER_RENDER_SYSTEM(JoltBoxDebugDrawSystem,     Priority::RenderStage::R08_Main);
REGISTER_RENDER_SYSTEM(JoltSphereDebugDrawSystem,  Priority::RenderStage::R08_Main);
REGISTER_RENDER_SYSTEM(JoltCapsuleDebugDrawSystem, Priority::RenderStage::R08_Main);
REGISTER_RENDER_SYSTEM(JoltMeshDebugDrawSystem,    Priority::RenderStage::R08_Main);