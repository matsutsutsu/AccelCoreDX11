#pragma once

#include "Engine/GamePlay/Core/DestroyTag.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "ECS/System/CCL_System.h"
#include <vector>


class HierarchyCleanupSystem : public CCL::ECS::IfSystem<
    HierarchyCleanupSystem, 
    CCL::ECS::Read<Tag::DestroyTag>> {
  public:
    HierarchyCleanupSystem();
    virtual ~HierarchyCleanupSystem() = default;

    // フレームの最後に実行され、タグ付きエンティティと子供を回収する
    void Update(float dt) override;

    // 外部（SceneManager等）から強制的にクリーンアップを走らせる関数
    static void ExecuteCleanup(CCL::ECS::Core::World& world);

private:
    // 再帰的に子供のIDをリストに追加するヘルパー関数
    // static関数から呼べるように、world を引数で受け取るようにする
    static void CollectChildrenRecursive(
        CCL::ECS::Core::World& world, CCL::ECS::EntityID currentID, std::vector<CCL::ECS::EntityID>& outList);
};

