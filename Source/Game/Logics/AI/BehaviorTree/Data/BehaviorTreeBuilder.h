#pragma once
#include <vector>
#include <memory>
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeData.h"

/**
 * @file BehaviorTreeBuilder.h
 * @brief ビヘイビアツリー構築用ビルダーの定義
 * * 役割：
 * 人間が理解しやすい階層的な記述（メソッドチェーン）をサポートし、
 * それをランタイム用の高速なフラット配列に変換するためのインターフェース。
 */


class BehaviorTreeBuilder {
private:
	// エディタ上のツリー構造を表すノード。BTAssetに変換する前の中間表現
    struct EditorNode {
        BTNodeType type;
        ActionID actionOrConditionId = 0;
        std::vector<std::unique_ptr<EditorNode>> children;  // 子をポインタで持つ（OOP的）
        EditorNode* parent = nullptr;
    };

    std::unique_ptr<EditorNode> m_root;
    EditorNode* m_currentNode = nullptr;

    void AddNode(BTNodeType type, ActionID id);

public:
    BehaviorTreeBuilder();

    BehaviorTreeBuilder& Selector();
    BehaviorTreeBuilder& Sequence();
    BehaviorTreeBuilder& Condition(ActionID conditionId);
    BehaviorTreeBuilder& Action(ActionID actionId);
    BehaviorTreeBuilder& End();

    void Bake(BTAsset& outAsset);
};
