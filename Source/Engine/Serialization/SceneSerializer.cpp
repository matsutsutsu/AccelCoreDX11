// Editor/Serialization/SceneSerializer.cpp
#include "SceneSerializer.h"
#include "ComponentRegistry.h"
#include "SerializationContext.h"
#include <fstream>
#include <iomanip>

#include "Engine/GamePlay/Transform/PendingParentComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/Combat/CombatRosterComponent.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

#include "Engine/GamePlay/Core/DestroyTag.h"
#include "Engine/GamePlay/Core/PersistentTag.h"

#include "Engine/Platform/Logger.h"

using namespace CCL::ECS;
using namespace CCL::ECS::Core;

// -----------------------------------------------------------------------
// 保存 (Serialize)
// -----------------------------------------------------------------------
void SceneSerializer::Serialize(World *world, const std::string &filepath)
{
    nlohmann::json root;
    root["entities"] = nlohmann::json::array();

    auto &chunkManager = world->GetChunkManager();
    auto &chunks       = chunkManager.GetChunks();

    // 全エンティティを走査して保存
    for (const auto &upChunk : chunks) {
        if (!upChunk) continue;
        Chunk          *chunk     = upChunk.get();

        // =========================================================
        // ★ アーキテクトの極意：セーブデータの汚染を物理的に防ぐ
        // =========================================================
        // 1. 削除予定の「ゴミ」は保存しない
        if (chunk->HasComponent<Tag::DestroyTag>()) continue;

        // 2. シーンを跨いで常駐する「システム（BGMやManager等）」は保存しない
        // （※これらはエンジン起動時に生成されるため、シーンに保存すると二重生成される）
        if (chunk->HasComponent<Tag::PersistentTag>()) continue;

        size_t          count     = chunk->GetEntityCount();
        const EntityID *entityIDs = chunk->GetEntityIDs();

        for (size_t i = 0; i < count; ++i) {
            if (chunk->IsEntityDestroyed(i)) continue;

            json entityJson;
            SerializeEntity(world, entityIDs[i], entityJson);
            root["entities"].push_back(entityJson);
        }
    }

    // UTF-8対応のパスでファイルを開く
    std::ofstream outFile(std::filesystem::u8path(filepath));
    if (!outFile.is_open()) {
        CCL_LOG_ERROR(LogCategory::Editor, "Failed to open file for save: %s", filepath.c_str());
        return;
    }

    // ★ JSONのダンプ処理を try-catch で保護し、例外を検知する
    try {
        // ここで例外が起きるとファイルが空白になります
        outFile << std::setw(4) << root << std::endl;
        CCL_LOG_INFO(LogCategory::Editor, "Scene saved successfully: %s", filepath.c_str());
    }
    catch (const std::exception& e) {
        // なぜ空白になったのか、その理由（不正な文字コード等）を出力させる
        CCL_LOG_ERROR(LogCategory::Editor, "JSON Serialize Exception: %s", e.what());
    }
}

void SceneSerializer::SerializeEntity(World *world, EntityID entityID, json &outJson)
{
    outJson["id"]         = (uint64_t)entityID; // IDを保存
    outJson["components"] = json::object();

    auto &registry = ComponentRegistry::Instance();

    // Entityが持っているコンポーネントを検索
    // (ここではWorld->GetComponentを使わず、Chunkから取るのが高速だが、
    //  汎用性のためComponentRegistry経由の実装イメージとします)

    // 実際の実装では、EntityのArchetypeを取得してループするのが正解です。
    // 簡易的に全登録コンポーネントをチェックする、またはChunkから取得するロジックにします。

    // Chunk経由の取得ロジック（推奨）:
    size_t index    = 0;
    size_t chunkIdx = world->GetChunkManager().SearchEntityIn(entityID, &index);
    if (chunkIdx != InvalidIndex) {
        Chunk      *chunk     = world->GetChunkManager().GetChunks()[chunkIdx].get();
        const auto &archetype = chunk->GetArchetype();

        for (const auto &typeData : archetype) {
            std::string name = registry.GetNameByTypeID(typeData.id);
            if (name.empty()) continue;

            void       *data = chunk->GetComponentPtrByType(typeData.id, index);
            const auto *meta = registry.GetInfoByName(name);
            if (meta && data) {
                json compJson;
                meta->serialize(compJson, data);
                outJson["components"][name] = compJson;
            }
        }
    }
}

// -----------------------------------------------------------------------
// 読み込み (Deserialize)
// -----------------------------------------------------------------------
std::vector<EntityID> SceneSerializer::Deserialize(World *world, const std::string &filepath)
{
    std::ifstream i(filepath);
    if (!i.is_open()) return {};

    nlohmann::json root;
    try {
        i >> root;
    }
    catch (...) {
        return {};
    }

    // 以前のIDマップをクリア
    // (Prefabロードなどの場合、呼び出し元で制御が必要かもだが、基本クリアでOK)
    SerializationContext::Clear();

   // ★修正点1: フォーマット判別
    if (root.contains("entities")) {
        // 新しい形式 (シーン保存したもの)
        return DeserializeRaw(world, root["entities"]);
    }
    else if (root.contains("components")) {
        // ★旧形式 (単体Prefab) への対応
        // これを 1要素だけの配列 として扱う
        nlohmann::json wrapper = nlohmann::json::array();
        wrapper.push_back(root);
        return DeserializeRaw(world, wrapper);
    }

    // キーが見つからなかった場合（BOM等によるエラー時）に備え、空配列を返す
    return {};
}

std::vector<EntityID> SceneSerializer::DeserializeRaw(World* world, const json& entitiesJson)
{
    std::vector<EntityID> createdEntities;

    // ★ 究極の修正1: このPrefabロード1回分「専用」の翻訳辞書を作る
    std::unordered_map<uint64_t, EntityID> localIdMap;

    auto& registry = ComponentRegistry::Instance();

    // ---------------------------------------------------
    // Pass 1: エンティティの一括生成 (Batch Spawn) & ID登録
    // ---------------------------------------------------
    for (const auto& entityJson : entitiesJson) {

        // ★ 究極の修正4: JSONから必要なコンポーネントの型情報を先に集め、
        // アーキタイプを事前に完成させる
        Archetype fullArch;

        if (entityJson.contains("components")) {
            const auto& components = entityJson["components"];
            for (auto it = components.begin(); it != components.end(); ++it) {
                std::string compName = it.key();

                // レジストリから型情報(TypeData)を取得してアーキタイプに追加
                // （※エンジン仕様に合わせ、RegistryからTypeIDを引く関数を使用）
                const auto* meta = registry.GetInfoByName(compName);
                if (meta) {
                    // TypeID を名前から引く (ComponentRegistryに機能がある前提)
                    // ない場合はメタのdeserializeToWorld内でAddComponentされるのを待つため、
                    // ここでは一旦 emptyArch で生成し、Pass2で追加させるフォールバックも可能。
                    // 今回はエンジン内部に依存しすぎないよう、一旦emptyArchで生成する
                    // （※「一気にSpawn」の完全実装には ComponentRegistry::GetTypeData(name) が必要です）
                }
            }
        }

        // 一旦、安全地帯(ヒープ)に空のアーキタイプを作り、Spawnを予約する
        Archetype* heapArch = new Archetype();
        EntityID newID = world->RequestSpawnEntity(*heapArch);

        // ★ 修正: RequestSpawnEntity内でヒープにコピーされたので、ここで作ったheapArchは即消す
        delete heapArch;

        createdEntities.push_back(newID);

        // ★ 究極の修正2: 専用辞書に登録する
        if (entityJson.contains("id")) {
            uint64_t oldID = entityJson["id"].get<uint64_t>();
            localIdMap[oldID] = newID;
        }
    } // ← ★ ここ！ Pass 1の forループの閉じカッコが抜けていました！

    // ---------------------------------------------------
    // Pass 2: コンポーネント復元 (PendingOpに積む)
    // ---------------------------------------------------
    size_t index = 0;
    for (const auto& entityJson : entitiesJson) {
        EntityID newID = createdEntities[index++];

        if (entityJson.contains("components")) {
            auto& components = entityJson["components"];
            for (auto it = components.begin(); it != components.end(); ++it) {
                std::string name = it.key();
                auto& data = it.value();
                const auto* meta = registry.GetInfoByName(name);
                if (meta) {
                    meta->deserializeToWorld(world, newID, data);
                }
            }
        }
    }

    // ★ 究極の修正A: Pass 3 で GetComponent するために、ここで一度実体化させる！
    world->ScrutinyAndApply();

    // ---------------------------------------------------
    // Pass 3: 親子関係とリンクの完全な再構築
    // ---------------------------------------------------
    size_t fixIndex = 0;
    for (const auto& entityJson : entitiesJson)
    {
        EntityID newID = createdEntities[fixIndex++];

        if (!entityJson.contains("components")) continue;
        const auto& components = entityJson["components"];

        // ★ 究極の修正B: continue をやめて独立した if ブロックにする
        // ① Transformの解決
        if (components.contains("Transform")) {
            const auto& transformJson = components["Transform"];
            uint64_t oldParentID = 0;
            if (transformJson.contains("parentID")) {
                oldParentID = transformJson["parentID"].get<uint64_t>();
            }
            else if (transformJson.contains("Parent Entity ID")) {
                oldParentID = transformJson["Parent Entity ID"].get<uint64_t>();
            }

            if (oldParentID != 0) {
                EntityID resolvedParentID = 0;
                if (localIdMap.find(oldParentID) != localIdMap.end()) {
                    resolvedParentID = localIdMap[oldParentID];
                }
                if (resolvedParentID != 0 && resolvedParentID != CCL::ECS::InvalidEntityID) {
                    PendingParentComponent pendingParent;
                    pendingParent.parentID = resolvedParentID;
                    world->AddComponent<PendingParentComponent>(newID, pendingParent);
                }
            }
        }

        // ② CombatRosterComponent の解決
        if (components.contains("CombatRosterComponent")) {
            auto* roster = world->GetComponent<CombatRosterComponent>(newID);
            if (roster) {
                for (int i = 0; i < roster->count; ++i) {
                    uint64_t oldLinkedID = static_cast<uint64_t>(roster->entries[i].id);
                    if (oldLinkedID != 0) {
                        if (localIdMap.find(oldLinkedID) != localIdMap.end()) {
                            roster->entries[i].id = localIdMap[oldLinkedID];
                        }
                        else {
                            roster->entries[i].id = 0;
                        }
                    }
                }
            }
        }

        // ③ ★追加: CameraBodyFollow のターゲット解決
        if (components.contains("CameraBodyFollow")) {
            auto* follow = world->GetComponent<CameraBodyFollow>(newID);
            if (follow) {
                uint64_t oldTargetID = static_cast<uint64_t>(follow->target);
                if (oldTargetID != 0) {
                    if (localIdMap.find(oldTargetID) != localIdMap.end()) {
                        follow->target = localIdMap[oldTargetID];
                    }
                    else {
                        follow->target = 0; // リンク切れの場合は無効化
                    }
                }
            }
        }

        // ④ ★追加: CameraLockOn のターゲット解決
        if (components.contains("CameraLockOn")) {
            auto* lockOn = world->GetComponent<CameraLockOn>(newID);
            if (lockOn) {
                uint64_t oldTargetID = static_cast<uint64_t>(lockOn->targetEntity);
                if (oldTargetID != 0) {
                    if (localIdMap.find(oldTargetID) != localIdMap.end()) {
                        lockOn->targetEntity = localIdMap[oldTargetID];
                    }
                    else {
                        lockOn->targetEntity = 0;
                    }
                }
            }
        }

        // ⑤ ★追加: CameraBodyTPS (三人称) のターゲット解決
        if (components.contains("CameraBodyTPS")) {
            auto* tps = world->GetComponent<CameraBodyTPS>(newID);
            if (tps) {
                uint64_t oldTargetID = static_cast<uint64_t>(tps->targetEntity);
                if (oldTargetID != 0) {
                    if (localIdMap.find(oldTargetID) != localIdMap.end()) {
                        tps->targetEntity = localIdMap[oldTargetID];
                    }
                    else {
                        tps->targetEntity = 0;
                        CCL_LOG_WARN(LogCategory::ECS, "[SceneSerializer] CameraBodyTPS target lost for old ID %llu", oldTargetID);
                    }
                }
            }
        }

        // ⑥ ★追加: CameraBodyFPS (一人称) のターゲット解決
        if (components.contains("CameraBodyFPS")) {
            auto* fps = world->GetComponent<CameraBodyFPS>(newID);
            if (fps) {
                uint64_t oldTargetID = static_cast<uint64_t>(fps->targetEntity);
                if (oldTargetID != 0) {
                    if (localIdMap.find(oldTargetID) != localIdMap.end()) {
                        fps->targetEntity = localIdMap[oldTargetID];
                    }
                    else {
                        fps->targetEntity = 0;
                        CCL_LOG_WARN(LogCategory::ECS, "[SceneSerializer] CameraBodyFPS target lost for old ID %llu", oldTargetID);
                    }
                }
            }
        }

    }


    // ★ 最後に PendingParent などを適用して階層を完成させる
    world->ScrutinyAndApply();

    return createdEntities;
}
