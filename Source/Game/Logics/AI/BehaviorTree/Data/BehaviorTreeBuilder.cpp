#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeBuilder.h"
#include "Engine/Platform/Logger.h"

/**
 * @file BehaviorTreeBuilder.cpp
 * @brief ビヘイビアツリーの配列化（Bake）処理の実装
 * * 役割：
 * 構築された中間ツリーを幅優先探索（BFS）で走査し、
 * 親子・兄弟関係が配列上で可能な限り「連続」するように配置する。
 * * なぜこの設計か（Why）：
 * CPUは次にアクセスするメモリアドレスを予測し、周辺データをまとめてキャッシュに載せる。
 * BFSによる配列化を行うことで、BTの評価時に「次に読む子ノード」が既にキャッシュに
 * 存在する確率を劇的に高めている。
 */

BehaviorTreeBuilder::BehaviorTreeBuilder() : m_root(nullptr), m_currentNode(nullptr) {}

void BehaviorTreeBuilder::AddNode(BTNodeType type, ActionID id) {
    auto newNode = std::make_unique<EditorNode>();
    newNode->type = type;
    newNode->actionOrConditionId = id;
    newNode->parent = m_currentNode;

    EditorNode* rawPtr = newNode.get();

    if (!m_root) {
        m_root = std::move(newNode);
        m_currentNode = rawPtr;
    }
    else {
        m_currentNode->children.push_back(std::move(newNode));
        if (type == BTNodeType::Selector || type == BTNodeType::Sequence) {
            m_currentNode = rawPtr;
        }
    }
}


BehaviorTreeBuilder& BehaviorTreeBuilder::Selector() { AddNode(BTNodeType::Selector, 0); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Sequence() { AddNode(BTNodeType::Sequence, 0); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Condition(ActionID conditionId) { AddNode(BTNodeType::Condition, conditionId); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Action(ActionID actionId) { AddNode(BTNodeType::Action, actionId); return *this; }

BehaviorTreeBuilder& BehaviorTreeBuilder::End() {
    if (m_currentNode && m_currentNode->parent) {
        m_currentNode = m_currentNode->parent;
    }
    else {
        CCL_LOG_ERROR(LogCategory::Game, "BehaviorTreeBuilder::End() called too many times!");
    }
    return *this;
}

void BehaviorTreeBuilder::Bake(BTAsset& outAsset) {
    if (!m_root) {
        CCL_LOG_WARN(LogCategory::Game, "Trying to bake an empty Behavior Tree.");
        return;
    }

    outAsset.nodes.clear();

    struct BFSItem {
        EditorNode* editorNode;
        BTNodeID flatArrayIndex;
    };

    std::vector<BFSItem> queue;

    BTNode rootRuntime{};
    rootRuntime.type = m_root->type;
    rootRuntime.actionOrConditionId = m_root->actionOrConditionId;
    outAsset.nodes.push_back(rootRuntime);

    queue.push_back({ m_root.get(), 0 });

    size_t head = 0;
    while (head < queue.size()) {
        auto [edNode, flatIndex] = queue[head++];

        if (edNode->children.empty()) {
            outAsset.nodes[flatIndex].childCount = 0;
            outAsset.nodes[flatIndex].firstChildIndex = 0;
            continue;
        }

        BTNodeID firstChildIdx = static_cast<BTNodeID>(outAsset.nodes.size());
        outAsset.nodes[flatIndex].firstChildIndex = firstChildIdx;
        outAsset.nodes[flatIndex].childCount = static_cast<uint8_t>(edNode->children.size());

        for (auto& childPtr : edNode->children) {
            BTNode childRuntime{};
            childRuntime.type = childPtr->type;
            childRuntime.actionOrConditionId = childPtr->actionOrConditionId;
            outAsset.nodes.push_back(childRuntime);

            queue.push_back({ childPtr.get(), static_cast<BTNodeID>(outAsset.nodes.size() - 1) });
        }
    }

    CCL_LOG_SUCCESS(LogCategory::Game, "BehaviorTree baked successfully. Total Nodes: %zu", outAsset.nodes.size());
}
