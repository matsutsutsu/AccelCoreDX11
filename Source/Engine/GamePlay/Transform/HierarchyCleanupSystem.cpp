#include "HierarchyCleanupSystem.h"
#include "Engine/GamePlay/Transform/TransformUpdateSystem.h" // DetachChildを使うため
#include "ECS/Core/CCL_World.h"
#include <algorithm> // for std::unique if needed
#include "Engine/Serialization/Factory/Prefab.h" // ReturnParticleToPoolを使うため
#include "Game/Utils/PooledParticleComponent.h"
#include <vector>

// 各システムの .cpp ファイルの上部に追加
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;
using namespace CCL::ECS::Core;

HierarchyCleanupSystem::HierarchyCleanupSystem() : IfSystem("HierarchyCleanupSystem") {}

void HierarchyCleanupSystem::Update(float dt)
{
    // 毎フレームの定期実行は、static関数に丸投げするだけ
    ExecuteCleanup(*_world);
}

// =========================================================================
// 実際のクリーンアップ処理 (static)
// =========================================================================
void HierarchyCleanupSystem::ExecuteCleanup(CCL::ECS::Core::World& world)
{
    // 削除対象を一時的に保持するリスト
    // ECSのメモリ構造を変更しながらのイテレーションは危険なため、
    // まず「誰を消すか」を確定させてから処理を実行します。
    std::vector<EntityID> executionList;
    executionList.reserve(64); // メモリ確保回数を減らすための予約

    // 1. DestroyTagがついている「削除起点」となるエンティティを見つける (チャンク手動走査)
    auto& chunks = world.GetChunkManager().GetChunks();
    for (auto& chunkPtr : chunks) {
        if (!chunkPtr) continue;
        auto* chunk = chunkPtr.get();

        // DestroyTagを持っていないチャンクは一瞬でスキップ（ForEachと同じ速度の秘密）
        if (!chunk->HasComponent<Tag::DestroyTag>()) continue;

        size_t count = chunk->GetEntityCount();
        const EntityID* entityIDs = chunk->GetEntityIDs();

        for (size_t i = 0; i < count; ++i) {
            if (chunk->IsEntityDestroyed(i)) continue;

            EntityID id = entityIDs[i];

            // まず自分自身をリストに追加
            executionList.push_back(id);

            // Transformコンポーネントを持っているなら、その子供たちも道連れにする
            if (world.HasComponent<TransformComponent>(id)) {
                CollectChildrenRecursive(world, id, executionList);
            }
        }
    }

    // 2. 重複の排除 (親と子が両方DestroyTagを持っていた場合などに備える)
    std::sort(executionList.begin(), executionList.end());
    executionList.erase(std::unique(executionList.begin(), executionList.end()), executionList.end());

    // 3. 実際の削除処理・プールへの返却
    for (EntityID targetID : executionList) {
        if (!world.IsEntityValid(targetID)) continue;

        bool isPooled = world.HasComponent<PooledParticleComponent>(targetID);

        // 親子関係の解除 (プールに戻す場合も、削除する場合も、リンクは切る必要がある)
        if (world.HasComponent<TransformComponent>(targetID)) {
            TransformUpdateSystem::DetachChild(world, targetID);
        }

        if (isPooled) {
            Prefab::ReturnParticleToPool(world, targetID);
            world.RequestRemoveComponent<Tag::DestroyTag>(targetID);
        }
        else {
            // 通常のエンティティなら完全に削除
            world.RequestDestroyEntity(targetID);
        }
    }
}
// =========================================================================
// 子供の再帰的収集 (static)
// =========================================================================
void HierarchyCleanupSystem::CollectChildrenRecursive(
    CCL::ECS::Core::World& world, EntityID currentID, std::vector<EntityID>& outList)
{
    // 現在のIDのTransformを取得
    auto* trans = world.GetComponent<TransformComponent>(currentID);
    if (!trans) return;

    // 子供リストの先頭を取得
    EntityID childID = trans->firstChildID;

    // 兄弟リンクを辿って全ての子供を走査
    while (childID != 0) {
        auto* childTrans = world.GetComponent<TransformComponent>(childID);
        if (!childTrans) break; // 安全策

        // リストに追加
        outList.push_back(childID);

        // 次の兄弟のIDを先に保存しておく（再帰で構造が変わるわけではないが念のため）
        EntityID nextSibling = childTrans->nextSiblingID;

        CollectChildrenRecursive(world, childID, outList);
        childID = nextSibling;
    }
}

// HierarchyCleanupSystem.cpp の末尾
REGISTER_LOGIC_SYSTEM(HierarchyCleanupSystem, Priority::LogicStage::L07_Cleanup);