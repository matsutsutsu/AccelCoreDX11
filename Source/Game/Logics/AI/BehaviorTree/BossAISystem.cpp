#include "Game/Logics/AI/BehaviorTree/BossAISystem.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeBuilder.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeLoader.h"
#include "Game/Logics/Character/Player/PlayerComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/Platform/Logger.h"
#include "Game/Core/SystemPriority.h" // LogicStage等の定義
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Logics/AI/BehaviorTree/Data/ActionRegistry.h"


/**
 * @file BossAISystem.cpp
 * @brief ボスAIシステムの実装
 * * 役割：
 * ForEachParallel を用いた超並列AI評価。
 * * なぜこの設計か（Why）：
 * 1. 仮想関数を排除した switch 分岐により、CPUパイプラインを停止させない。
 * 2. 再帰呼び出しを排除し、スタックを自前管理（固定長配列）することで、
 * 関数呼び出しのオーバーヘッドをゼロにし、メモリ安全性と速度を両立している。
 * 3. 60fps維持のため、AIは「コマンドを発行するだけ」に留め、
 * 物理演算やアニメーションとの依存を最小限に抑えている。
 */

 // 規約に基づきcpp側でusing namespace
using namespace CCL::ECS;

BossAISystem::BossAISystem() : IfSystem("BossAISystem") {}


void BossAISystem::Update(float dt)
{
    // =========================================================
    // 1. ターゲット（プレイヤー）の検索フェーズ（シングルスレッド）
    // =========================================================
    EntityID activePlayerId = CCL::ECS::InvalidEntityID;

    auto playerView = _world->View<PlayerComponent,TransformComponent>();
    for (auto p : playerView) {
        activePlayerId = p;
        break;
    }

    // プレイヤーが見つからなければAIは思考を停止するか、待機する
    if (activePlayerId == CCL::ECS::InvalidEntityID) return;


    // ★ 並列実行 (ForEachParallel) 
    // テンプレート引数とラムダの引数型を【完全に一致】させなければコンパイルエラーになる
    ForEachParallel([this,dt,activePlayerId](BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp) {

        // ========================================================
        // 1. 動的ロード
        // ========================================================
        if (btComp.assetPath != btComp.loadedAssetPath) {
            
            // パスが書き換わった瞬間（または初回起動時）だけロードを実行
            if (!btComp.assetPath.empty()) {
                BehaviorTreeLoader::LoadFromJson(btComp.assetPath, *btComp.sharedAsset);
            }
            
            // 重要：成功・失敗に関わらず「このパスについては処理した」と記憶させる。
            // これにより、毎フレームの無駄なファイルアクセスとログスパムを完全に遮断する。
            btComp.loadedAssetPath = btComp.assetPath;
        }

        if (!btComp.sharedAsset || btComp.sharedAsset->nodes.empty()) return;

        // ========================================================
        // 2. メモリバッファの確保（エディタ可視化＆タイマー用）
        // ========================================================
        size_t nodeCount = btComp.sharedAsset->nodes.size();
        if (btComp.debugNodeStates.size() < nodeCount) {
            btComp.debugNodeStates.resize(nodeCount, BTDebugState::None);
            btComp.nodeTimers.resize(nodeCount, 0.0f);        
        }

        // デバッグ状態の初期化と、タイマーの進行（クールダウンの消化）
        std::fill(btComp.debugNodeStates.begin(), btComp.debugNodeStates.end(), BTDebugState::None);
        for (float& timer : btComp.nodeTimers) {
            if (timer > 0.0f) timer -= dt;
        }


        // 前フレームのフラグを全てリセット
        cmdComp.requestMeleeAttack = false;
        cmdComp.requestCharge = false;
        cmdComp.requestGuard = false;
        cmdComp.requestMove = false;
        cmdComp.targetPlayerId = activePlayerId;



        // ========================================================
        // 4. ツリーの評価実行
        // ========================================================
        btComp.runningNodeId = 0xFFFF; // 評価前にRunning状態をリセット
        
        if (btComp.sharedAsset && !btComp.sharedAsset->nodes.empty()) {
            EvaluateTreeRecursive(0, btComp, cmdComp, *btComp.sharedAsset, dt);
        }


    });
}

// ========================================================================
// ★ カプセル化された再帰評価エンジン（デコレーター＆デバッガー対応版）
// ========================================================================
BTNodeState BossAISystem::EvaluateTreeRecursive(
    BTNodeID nodeId,
    BehaviorTreeComponent& btComp,
    BossCommandComponent& cmdComp,
    const BTAsset& asset,
    float dt)
{
    const BTNode& node = asset.nodes[nodeId];
    BTNodeState result = BTNodeState::Failure;

    switch (node.type) {
    case BTNodeType::Selector: {
        for (int i = 0; i < node.childCount; ++i) {
            result = EvaluateTreeRecursive(node.firstChildIndex + i, btComp, cmdComp, asset, dt);
            if (result != BTNodeState::Failure) break;
        }
        break;
    }
    case BTNodeType::Sequence: {
        for (int i = 0; i < node.childCount; ++i) {
            result = EvaluateTreeRecursive(node.firstChildIndex + i, btComp, cmdComp, asset, dt);
            if (result != BTNodeState::Success) break;
        }
        break;
    }
    case BTNodeType::Condition: {
        result = EvaluateCondition(node.actionOrConditionId, btComp) ? BTNodeState::Success : BTNodeState::Failure;
        break;
    }
    case BTNodeType::Action: {
        result = ExecuteAction(node.actionOrConditionId, btComp, cmdComp, nodeId, dt);
        break;
    }
    case BTNodeType::Decorator: {
        // クールダウン中なら問答無用でFailureを返す
        if (node.decoratorType == BTDecoratorType::Cooldown && btComp.nodeTimers[nodeId] > 0.0f) {
            result = BTNodeState::Failure;
            break;
        }

        if (node.childCount > 0) {
            // Retryデコレーターの場合は、失敗しても指定回数ループして再評価する
            if (node.decoratorType == BTDecoratorType::Retry) {
                int retryCount = static_cast<int>(node.decoratorParam);
                for (int r = 0; r < retryCount; ++r) {
                    result = EvaluateTreeRecursive(node.firstChildIndex, btComp, cmdComp, asset, dt);
                    if (result != BTNodeState::Failure) break; // 成功かRunningならリトライ終了
                }
            }
            else {
                // 通常の評価
                result = EvaluateTreeRecursive(node.firstChildIndex, btComp, cmdComp, asset, dt);
            }

            // デコレーターの特殊処理（結果の加工）
            if (node.decoratorType == BTDecoratorType::Inverter) {
                if (result == BTNodeState::Success) result = BTNodeState::Failure;
                else if (result == BTNodeState::Failure) result = BTNodeState::Success;
            }
            else if (node.decoratorType == BTDecoratorType::Cooldown && result == BTNodeState::Success) {
                // 成功した瞬間にクールダウンタイマーをセット
                btComp.nodeTimers[nodeId] = node.decoratorParam;
            }
        }
        break;
    }
    }

    // =========================================================
    //  BTNodeState (論理型) を BTDebugState (表示型) に安全に変換
    // =========================================================
    BTDebugState dbgState = BTDebugState::None;
    if (result == BTNodeState::Success) {
        dbgState = BTDebugState::Success;
    } else if (result == BTNodeState::Failure) {
        dbgState = BTDebugState::Failure;
    } else if (result == BTNodeState::Running) {
        dbgState = BTDebugState::Running;
    }

    // エディタのデバッガー向けに結果を保存
    btComp.debugNodeStates[nodeId] = dbgState;

    if (result == BTNodeState::Running) {
        btComp.runningNodeId = nodeId;
    }

    return result;
}

// 条件を評価する関数。条件IDに応じて、Blackboardの情報を参照して真偽を返す。
bool BossAISystem::EvaluateCondition(ActionID condId, const BehaviorTreeComponent& btComp)
{
    switch (condId) {
    case BossAI_ID::Cond_RangeClose:
        return btComp.blackboard.distanceToTarget < 5.0f; // 0〜5秒の間は成功
    case BossAI_ID::Cond_RangeMedium:
        return btComp.blackboard.distanceToTarget >= 5.0f && btComp.blackboard.distanceToTarget < 10.0f; // 5〜10秒の間は成功
    case BossAI_ID::Cond_IsPhase2:
        return btComp.blackboard.currentPhase >= 2; // 後半戦かどうか
    default: return false;
    }
}

// 行動を評価する関数。行動IDに応じて、cmdCompに行動要求を書き込む。
BTNodeState BossAISystem::ExecuteAction(ActionID actionId, BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp, BTNodeID nodeId, float dt)
{
    // Wait（待機）アクションのロジック
    // デコレーター用に確保した nodeTimers を再利用して待機時間を管理するスマートな設計
    if (actionId == BossAI_ID::Act_Wait1s || actionId == BossAI_ID::Act_Wait3s) {
        // 新しくこのノードに入ってきた瞬間（Running開始時）にタイマーをセットする
        if (btComp.runningNodeId != nodeId) {
            float waitTime = (actionId == BossAI_ID::Act_Wait3s) ? 3.0f : 1.0f;
            btComp.nodeTimers[nodeId] = waitTime;
            CCL_LOG_INFO(LogCategory::AI, "[BossAI] Wait Started: %.1f sec", waitTime);
        }
        
        // 毎フレームタイマーが減っていき、0になったら完了（Success）
        if (btComp.nodeTimers[nodeId] > 0.0f) {
            return BTNodeState::Running;
        }
        return BTNodeState::Success;
    }

    // 既存のログ処理
    if (btComp.lastLogActionId != actionId) {
        switch (actionId) {
        case BossAI_ID::Act_Move:
            CCL_LOG_INFO(LogCategory::AI, "[BossAI] State Changed -> MOVE"); break;
        case BossAI_ID::Act_MeleeAttack:
            CCL_LOG_SUCCESS(LogCategory::AI, "[BossAI] State Changed -> MELEE ATTACK"); break;
        case BossAI_ID::Act_ChargeAttack:
            CCL_LOG_WARN(LogCategory::AI, "[BossAI] State Changed -> CHARGE ATTACK"); break;
        case BossAI_ID::Act_DroneOrbit:
            CCL_LOG_WARN(LogCategory::AI, "[BossAI] State Changed -> DRONE ORBIT (円陣展開)"); break;
        case BossAI_ID::Act_DroneSequential:
            CCL_LOG_WARN(LogCategory::AI, "[BossAI] State Changed -> DRONE SEQUENTIAL (順番突撃)"); break;
        case BossAI_ID::Act_DroneDeathRing:
            CCL_LOG_WARN(LogCategory::AI, "[BossAI] State Changed -> DRONE DEATH RING (処刑の輪)"); break;
        case BossAI_ID::Act_DroneAegisShield:
            CCL_LOG_WARN(LogCategory::AI, "[BossAI] State Changed -> DRONE AEGIS SHIELD (絶対防衛)"); break;
        }
        btComp.lastLogActionId = actionId;
    }

    switch (actionId) {
    case BossAI_ID::Act_Move:
        cmdComp.requestMove = true;
        return BTNodeState::Running;
    case BossAI_ID::Act_MeleeAttack:
        cmdComp.requestMeleeAttack = true;
        return BTNodeState::Success;
    case BossAI_ID::Act_ChargeAttack:
        cmdComp.requestCharge = true;
        return BTNodeState::Running;
        // ドローンに対する指示を発行（AIは指示を出すだけで、実際の動作はDroneSystemが行う）
    case BossAI_ID::Act_DroneOrbit:
        cmdComp.requestFormation = DroneFormationType::OrbitCircle;
        return BTNodeState::Success;
    case BossAI_ID::Act_DroneSequential:
        cmdComp.requestFormation = DroneFormationType::SequentialAttack;
        return BTNodeState::Success;
    case BossAI_ID::Act_DroneDeathRing:
        cmdComp.requestFormation = DroneFormationType::DeathRing;
        return BTNodeState::Success;
    case BossAI_ID::Act_DroneAegisShield:
        cmdComp.requestFormation = DroneFormationType::AegisShield;
        return BTNodeState::Success;
    default:
        return BTNodeState::Failure;
    }
}

// 規約に基づき末尾でシステム自動登録。
// 実行タイミングは「L02_Update (AI、移動、弾の更新など並列化の主戦場)」を指定。
// ※プロジェクト側に REGISTER_LOGIC_SYSTEM マクロが定義されている前提
REGISTER_LOGIC_SYSTEM(BossAISystem, Priority::LogicStage::L02_Update);