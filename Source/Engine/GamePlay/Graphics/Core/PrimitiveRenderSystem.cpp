#include "PrimitiveRenderSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Renderer/PrimitiveRenderer.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"


using namespace CCL::ECS;

PrimitiveRenderSystem::PrimitiveRenderSystem() : IfSystem("PrimitiveRenderSystem") {}

void PrimitiveRenderSystem::Update(float dt)
{
    // Worldからレンダラーのポインタを取得
    if (!_world->HasResource<PrimitiveRenderer*>()) return;
    auto* renderer = _world->GetResource<PrimitiveRenderer*>();
    if (!renderer) return;

    ForEachParallel([&](const TransformComponent& trans, const PrimitiveComponent& prim, const VisibilityComponent& vis) { // ★追加

        // ここでカリング結果を見てスキップ
        if (!vis.isVisible) return;

        DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(&trans.worldMatrix);
        DirectX::XMMATRIX scaleMat = DirectX::XMMatrixIdentity();

        // ゼロスケールによるNaN爆弾（画面崩壊）を防ぐ安全値
        float minS = 0.0001f;
        float r = (std::max)(prim.radius, minS);
        float h = (std::max)(prim.height, minS);

        // =========================================================
        // 1. 図形ごとのスケール計算
        // =========================================================
        switch (prim.type) {
        case PrimitiveType::Box:
            scaleMat = DirectX::XMMatrixScaling(
                (std::max)(prim.size.x, minS),
                (std::max)(prim.size.y, minS),
                (std::max)(prim.size.z, minS));
            break;

        case PrimitiveType::Sphere:
            scaleMat = DirectX::XMMatrixScaling(r, r, r);
            break;

        case PrimitiveType::Cylinder:
        case PrimitiveType::Cone:
            // 円柱と円錐はそのまま 半径・高さ・半径 を適用
            scaleMat = DirectX::XMMatrixScaling(r, h, r);
            break;

        case PrimitiveType::Capsule:
        {
            scaleMat = DirectX::XMMatrixIdentity();
            break;
        }
        }

        // =========================================================
        // 2. 行列の合成とフラグの埋め込み
        // =========================================================
        DirectX::XMMATRIX finalWorldMat = scaleMat * worldMat;
        DirectX::XMFLOAT4X4 finalWorld;
        DirectX::XMStoreFloat4x4(&finalWorld, finalWorldMat);

        // ★究極のハック：行列の使われない空き部屋（右端の1列）にパラメータを直接隠して送る
        if (prim.type == PrimitiveType::Capsule) {
            float totalHeight = (std::max)(h, r * 2.0f);
            float cylinderHeight = totalHeight - (r * 2.0f);

            finalWorld._14 = 1.0f;           // [0].w : カプセルであるというフラグ
            finalWorld._24 = r;              // [1].w : カプセルの半径 (Radius)
            finalWorld._34 = cylinderHeight; // [2].w : 円柱部分の長さ (Height)
        }
        else {
            finalWorld._14 = 0.0f;
        }

        // =========================================================
        // 3. レンダラーへの描画登録
        // =========================================================
        if (prim.isWireframe) {
            switch (prim.type) {
            case PrimitiveType::Box:      renderer->DrawWireframeBox(finalWorld, prim.color); break;
            case PrimitiveType::Sphere:   renderer->DrawWireframeSphere(finalWorld, prim.color); break;
            case PrimitiveType::Cylinder: renderer->DrawWireframeCylinder(finalWorld, prim.color); break;
            case PrimitiveType::Capsule:  renderer->DrawWireframeCapsule(finalWorld, prim.color); break;
            case PrimitiveType::Cone:     renderer->DrawWireframeCone(finalWorld, prim.color); break;
            }
        }
        else {
            switch (prim.type) {
            case PrimitiveType::Box:      renderer->DrawBox(finalWorld, prim.color); break;
            case PrimitiveType::Sphere:   renderer->DrawSphere(finalWorld, prim.color); break;
            case PrimitiveType::Cylinder: renderer->DrawCylinder(finalWorld, prim.color); break;
            case PrimitiveType::Capsule:  renderer->DrawCapsule(finalWorld, prim.color); break;
            case PrimitiveType::Cone:     renderer->DrawCone(finalWorld, prim.color); break;
            }
        }
        });
}

REGISTER_RENDER_SYSTEM(PrimitiveRenderSystem, Priority::RenderStage::R08_Main);