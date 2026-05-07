#pragma once
#include <cstdint>
#include <vector>

/**
 * @file BehaviorTreeData.h
 * @brief ビヘイビアツリー（BT）の静的データ構造の定義
 * * 役割：
 * AIの論理的な「思考構造」をメモリ上に定義する。
 * * 設計思想（DOD）:
 * 伝統的なポインタベースの木構造（OOP）を完全に排除。
 * すべてのノードを固定長の構造体（BTNode）として定義し、配列（std::vector）に配置する。
 * これにより、ポインタジャンプ（Pointer Chasing）をなくし、L1/L2キャッシュへの
 * プリフェッチ効率を最大化している。
 */


// 厳格な型付け
using BTNodeID = uint16_t;
using ActionID = uint16_t;
using AssetID = uint32_t;

// デバッグ用のノード状態（シリアライズ不要、実行時の可視化専用）
enum class BTDebugState : uint8_t {
    None = 0,
    Success,
    Failure,
    Running
};

// デコレーターの種類
enum class BTDecoratorType : uint8_t {
    Inverter = 0, // 結果を反転 (Success <-> Failure)
    Cooldown = 1, // クールダウン (指定秒数経過するまでFailure)
    Retry = 2,     // 失敗しても指定回数リトライする
    Async = 3     // 非同期実行（筋肉を待たずに即Successを返す）
};


// ノードの種類
// Selector: 子ノードを順番に評価し、最初に成功した子ノードの結果を返す
// Sequence: 子ノードを順番に評価し、最初に失敗した子ノードの結果を返す
// Condition: 条件を評価し、成功/失敗を返す
// Action: 行動を実行し、成功/失敗/実行中を返す
enum class BTNodeType : uint8_t {
    Selector,
    Sequence,
    Condition,
    Action,
    Decorator
};

// ノードの評価結果
// Success: 成功
// Failure: 失敗
// Running: 実行中（Actionノードのみが返す可能性がある状態。次フレームも同じノードを評価し続ける必要があることを示す）
enum class BTNodeState : uint8_t {
    Success,
    Failure,
    Running
};

// 1つのノード定義。メモリレイアウトを意識し、8バイトに収める。
struct BTNode {
	BTNodeType type;                // ノードの種類（Selector, Sequence, Condition, Action）
    ActionID   actionOrConditionId; // Action/Conditionの種類を判別するID

    // デコレーター用パラメータ
    BTDecoratorType decoratorType;
    float decoratorParam; // クールダウン秒数やリトライ回数など


    // 確実に子ノードのID（配列のインデックス）を保持するリストに変更
    std::vector<BTNodeID> children;
};

// 全Entityで共有される不変データ（アセット）
struct BTAsset {
    AssetID id;
    std::vector<BTNode> nodes;
};
