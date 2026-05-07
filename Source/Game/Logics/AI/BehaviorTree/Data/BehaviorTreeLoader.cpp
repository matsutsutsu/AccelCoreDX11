#include "BehaviorTreeLoader.h"
#include "Engine/Platform/Logger.h"
#include <fstream>
#include <json.hpp>

using json = nlohmann::json;


bool BehaviorTreeLoader::LoadFromJson(const std::string& path, BTAsset& outAsset) {
    std::ifstream file(path);
    if (!file.is_open()) {
        CCL_LOG_ERROR(LogCategory::AI, "Failed to open BT file: %s", path.c_str());
        return false;
    }

    nlohmann::json j;
    try {
        file >> j;
    }
    catch (const std::exception& e) {
        CCL_LOG_ERROR(LogCategory::AI, "JSON Parse Error in %s: %s", path.c_str(), e.what());
        return false;
    }

    outAsset.id = j.value("assetId", 0u);
    outAsset.nodes.clear();

    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (auto& jNode : j["nodes"]) {
            BTNode node;
            node.type = StringToNodeType(jNode.value("type", "Action"));
            node.actionOrConditionId = static_cast<ActionID>(jNode.value("actionOrConditionId", 0));

            // デコレーター情報の読み込み
            node.decoratorType = static_cast<BTDecoratorType>(jNode.value("decoratorType", 0));
            node.decoratorParam = jNode.value("decoratorParam", 0.0f);

            // JSONから "children": [1, 4] のような配列を読み込む
            if (jNode.contains("children") && jNode["children"].is_array()) {
                for (auto& childId : jNode["children"]) {
                    node.children.push_back(childId.get<BTNodeID>());
                }
            }

            outAsset.nodes.push_back(node);
        }
    }

    CCL_LOG_SUCCESS(LogCategory::AI, "BT Asset Loaded: %s (Nodes: %zu)", path.c_str(), outAsset.nodes.size());
    return true;
}

BTNodeType BehaviorTreeLoader::StringToNodeType(const std::string& typeStr) {
    if (typeStr == "Selector")  return BTNodeType::Selector;
    if (typeStr == "Sequence")  return BTNodeType::Sequence;
    if (typeStr == "Condition") return BTNodeType::Condition;
    if (typeStr == "Decorator") return BTNodeType::Decorator;
    return BTNodeType::Action;
}