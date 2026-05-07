// Editor/Serialization/PrefabSerializer.cpp
#include "PrefabSerializer.h"
#include "ComponentRegistry.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "SceneSerializer.h" // 内部のSerializeEntityを使いたいので
#include <fstream>
#include <iomanip>
#include <queue>

using namespace CCL::ECS;
using namespace CCL::ECS::Core;

void PrefabSerializer::Save(
    const std::string &filepath, World *world, EntityID rootEntity, const std::string &name)
{
    if (rootEntity == 0) return;

    nlohmann::json root;
    root["name"]     = name;
    root["entities"] = nlohmann::json::array();

    // 1. 保存対象のエンティティリストを作成 (自分 + 全子孫)
    std::vector<EntityID> entitiesToSave;
    std::queue<EntityID>  q;
    q.push(rootEntity);

    // 子エンティティを見つけるには全探索が必要 (Transformにchildrenリストがないため)
    // エディタ機能なので多少重くてもOK
    // ※高速化するならHierarchySystemなどで親子キャッシュを持つべきですが、一旦全探索で実装
    auto &chunkManager = world->GetChunkManager();
    auto &chunks       = chunkManager.GetChunks();

    // ルートを追加
    entitiesToSave.push_back(rootEntity);

    // 再帰的に子供を探す
    // (注意: 現在のTransformComponentの仕様では子から親への参照(parentID)しかないため、
    //  あるエンティティの子供を知るには全エンティティを走査して parentID == currentID
    //  を探す必要がある)

    // 全エンティティの親IDリストをキャッシュ作成（最適化）
    std::vector<std::pair<EntityID, EntityID>> allParentLinks; // child, parent
    for (const auto &upChunk : chunks) {
        if (!upChunk) continue;
        Chunk          *chunk = upChunk.get();
        size_t          cnt   = chunk->GetEntityCount();
        const EntityID *ids   = chunk->GetEntityIDs();

        // Transformコンポーネントを持つか？
        // 簡易的にGetComponentPtrByTypeで確認
        // (TypeIDはTransformComponent::TypeID()とかで取れる想定)
        // ここでは愚直にComponentRegistryは使わず、World->GetComponentを使う
        // (Chunk走査の方が速いが、コードの簡潔さを優先)
        for (size_t i = 0; i < cnt; ++i) {
            if (chunk->IsEntityDestroyed(i)) continue;
            EntityID id = ids[i];
            auto    *t  = world->GetComponent<TransformComponent>(id);
            if (t && t->parentID != 0) {
                allParentLinks.push_back({id, t->parentID});
            }
        }
    }

    // BFSで子孫を集める
    size_t index = 0;
    while (index < entitiesToSave.size()) {
        EntityID parent = entitiesToSave[index++];
        for (auto &link : allParentLinks) {
            if (link.second == parent) {
                entitiesToSave.push_back(link.first);
            }
        }
    }

    // 2. リストアップしたエンティティをJSON化
    // SceneSerializerのロジックと似ていますが、対象を限定します
    for (EntityID id : entitiesToSave) {
        nlohmann::json entityJson;

        // SceneSerializerはprivateなので直接呼べない場合、似た処理を書くか、
        // SceneSerializerに public static Helper を作るのが良いです。
        // ここではロジックを再記述します。

        entityJson["id"]         = (uint64_t)id;
        entityJson["components"] = nlohmann::json::object();

        auto &registry = ComponentRegistry::Instance();

        // コンポーネント保存
        size_t idx;
        size_t cIdx = chunkManager.SearchEntityIn(id, &idx);
        if (cIdx != InvalidIndex) {
            Chunk *chunk = chunkManager.GetChunks()[cIdx].get();
            for (const auto &td : chunk->GetArchetype()) {
                std::string cName = registry.GetNameByTypeID(td.id);
                if (cName.empty()) continue;

                void       *data = chunk->GetComponentPtrByType(td.id, idx);
                const auto *meta = registry.GetInfoByName(cName);
                if (meta && data) {
                    nlohmann::json compJson;
                    meta->serialize(compJson, data);
                    entityJson["components"][cName] = compJson;
                }
            }
        }

        root["entities"].push_back(entityJson);
    }

    // 3. 書き出し
    std::ofstream o(filepath);
    if (o.is_open()) {
        o << std::setw(4) << root << std::endl;
    }
}