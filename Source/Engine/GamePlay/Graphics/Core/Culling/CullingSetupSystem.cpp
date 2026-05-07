#include "CullingSetupSystem.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"



using namespace CCL::ECS;

// =======================================================================
// Modelのセットアップ
// =======================================================================
ModelCullingSetupSystem::ModelCullingSetupSystem() : IfSystem("ModelCullingSetupSystem") {}

void ModelCullingSetupSystem::Update(float dt)
{
    std::vector<EntityID> needsBoundingBox;
    std::vector<EntityID> needsVisibility;

    // 1. 足りないコンポーネントを【個別に】調査する
    ForEachWithID([&](EntityID id, const ModelComponent& model) {
        if (!_world->HasComponent<BoundingBoxComponent>(id)) {
            needsBoundingBox.push_back(id);
        }
        if (!_world->HasComponent<VisibilityComponent>(id)) {
            needsVisibility.push_back(id);
        }
        });

    // 2. 足りないものだけを【個別に】付与する
    for (auto id : needsBoundingBox) {
        _world->AddComponent<BoundingBoxComponent>(id);
    }
    for (auto id : needsVisibility) {
        _world->AddComponent<VisibilityComponent>(id);
    }
}

// =======================================================================
// Primitiveのセットアップ
// =======================================================================
PrimitiveCullingSetupSystem::PrimitiveCullingSetupSystem() : IfSystem("PrimitiveCullingSetupSystem") {}

void PrimitiveCullingSetupSystem::Update(float dt)
{
    std::vector<EntityID> needsBoundingBox;
    std::vector<EntityID> needsVisibility;

    // 1. 足りないコンポーネントを【個別に】調査する
    ForEachWithID([&](EntityID id, const PrimitiveComponent& prim) {
        if (!_world->HasComponent<BoundingBoxComponent>(id)) {
            needsBoundingBox.push_back(id);
        }
        if (!_world->HasComponent<VisibilityComponent>(id)) {
            needsVisibility.push_back(id);
        }
        });

    // 2. 足りないものだけを【個別に】付与する
    for (auto id : needsBoundingBox) {
        _world->AddComponent<BoundingBoxComponent>(id);
    }
    for (auto id : needsVisibility) {
        _world->AddComponent<VisibilityComponent>(id);
    }
}

// 実行順序：カリング計算（AABB生成や交差判定）の前に登録する
// ※必要に応じてあなたのSystemPriorityの設定に合わせて変更してください
REGISTER_RENDER_SYSTEM(ModelCullingSetupSystem, Priority::RenderStage::R05_Culling);
REGISTER_RENDER_SYSTEM(PrimitiveCullingSetupSystem, Priority::RenderStage::R05_Culling);