// Editor/Serialization/SceneSerializer.h
#pragma once
#include "ECS/Core/CCL_World.h"
#include <json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

class SceneSerializer {
public:
    // =========================================================
    // プレハブ / サブシーンAPI
    // =========================================================

    // 指定したパスにエンティティ群を保存
    static void Serialize(CCL::ECS::Core::World* world, const std::string& filepath);

    // ファイルから現在の世界に追加で読み込む (Worldはクリアされない)
    static std::vector<CCL::ECS::EntityID> Deserialize(
        CCL::ECS::Core::World* world, const std::string& filepath);

    // JSONオブジェクトから直接読み込む
    static std::vector<CCL::ECS::EntityID> DeserializeRaw(
        CCL::ECS::Core::World* world, const json& entitiesJson);

private:
    // 内部ヘルパー
    static void SerializeEntity(
        CCL::ECS::Core::World* world, CCL::ECS::EntityID entity, json& outJson);
};