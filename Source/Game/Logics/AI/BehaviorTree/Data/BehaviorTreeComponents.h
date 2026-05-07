#pragma once
#include <cstdint>
#include <memory>
#include "SimpleMath.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeData.h"
#include "Game/Logics/AI/BehaviorTree/Drone/DroneComponent.h"

/**
 * @file BehaviorTreeComponents.h
 * @brief ECSエンティティが保持するAIの状態管理コンポーネント
 * * 役割：
 * 各エンティティ固有の「現在の思考位置」や「判断材料（Blackboard）」を管理する。
 * * 設計思想：
 * 「思考（Data）」と「状態（Component）」を分離。
 * コンポーネントには実行時のみ必要な最小限の状態のみを持たせ、
 * 巨大な木構造自体はアセットとして全Entityで共有する（メモリ節約）。
 */

 // AIの判断材料を格納するブラックボード（Blackboard）
struct AIBlackboard {
    DirectX::SimpleMath::Vector3 targetPosition; // ターゲットの座標
    float distanceToTarget = 0.0f;               // ターゲットとの距離
    float healthPercentage = 100.0f;             // 自分の残りHP

    // ボスの現在のフェーズ（1=前半戦, 2=後半戦）
    int currentPhase = 1;
};

// AIの内部状態とブラックボード（直列化対象）
// 例えば、どのノードを実行中か、どのアセットを使うかなどの情報を持つ
struct BehaviorTreeComponent {
    std::string assetPath = "Assets/AI/BossAI.json";     // 読み込むJSONのパス
    std::string loadedAssetPath = "";                    // エンジンが「現在メモリに読み込んでいる」パス

    AssetID assetId = 0;        // どの知能（BTAsset）を使うか
    BTNodeID runningNodeId = 0; // 現在実行中のノード    
    ActionID lastLogActionId = 0xFFFF;  // ログ出力制御用（前回実行したアクションIDを記憶）

    // 実体ではなく、共有ポインタに変更する（容量削減！）
    std::shared_ptr<BTAsset> sharedAsset;

    // 判断材料をここに内包する（Blackboard）
    AIBlackboard blackboard;

    // エディタ可視化用の状態バッファ (配列サイズは BTAsset のノード数と同じになる)
    std::vector<BTDebugState> debugNodeStates;

    // デコレーター（クールダウン等）のタイマーを記憶するバッファ
    std::vector<float> nodeTimers;

};



// AIシステムからの「出力」を受け取るコンポーネント（毎フレームリセットされる使い捨てデータ）
// 例えば、AIが「近接攻撃してほしい」「ここに移動してほしい」といった命令をこのコンポーネントに書き込む
struct BossCommandComponent {
    bool requestMeleeAttack = false; // 近接攻撃をしてほしい！
    bool requestCharge = false;      // 突進してほしい！
    bool requestGuard = false;
    bool requestMove = false; // ★この1行を追加！
    DirectX::SimpleMath::Vector3 moveTarget; // ここに移動してほしい！

    DroneFormationType requestFormation = DroneFormationType::Hidden;
    CCL::ECS::EntityID targetPlayerId = CCL::ECS::InvalidEntityID; // 誰を狙うか
};