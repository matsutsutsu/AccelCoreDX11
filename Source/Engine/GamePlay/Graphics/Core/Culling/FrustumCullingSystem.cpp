#include "FrustumCullingSystem.h"
#include "Engine/Graphics/Core/Camera.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"


using namespace CCL::ECS;

FrustumCullingSystem::FrustumCullingSystem() : IfSystem("FrustumCullingSystem") {}

void FrustumCullingSystem::Update(float dt)
{
    if (!_world->HasResource<Camera*>()) return;
    auto* camera = _world->GetResource<Camera*>();
    if (!camera) return;

    // カメラ自身が計算済みの、絶対的に正確な最新Frustumを取得
    const DirectX::BoundingFrustum& frustum = camera->GetFrustum();

    ForEachParallel([&](const BoundingBoxComponent& bounds, VisibilityComponent& vis) {
        vis.isVisible = frustum.Intersects(bounds.worldAABB);
        });
}

REGISTER_RENDER_SYSTEM(FrustumCullingSystem, Priority::RenderStage::R05_Culling);