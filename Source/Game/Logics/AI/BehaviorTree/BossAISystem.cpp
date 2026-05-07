#include "Game/Logics/AI/BehaviorTree/BossAISystem.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeBuilder.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeLoader.h"
#include "Game/Logics/Character/Player/TPS/TPSPlayerComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/Platform/Logger.h"
#include "Game/Core/SystemPriority.h" // LogicStage等の定義
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Logics/AI/BehaviorTree/Data/ActionRegistry.h"
#include <SimpleMath.h>
#include <imgui.h>
#include "Engine/Graphics/Core/Camera.h"

using namespace DirectX::SimpleMath;


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

BossAISystem::BossAISystem() : IfSystem("BossAISystem") 
{
	hasGui = true; // デバッグGUIを持つことを宣言
}


void BossAISystem::Update(float rawDt)
{
    // =========================================================
    // 1. ターゲット（プレイヤー）の検索フェーズ（シングルスレッド）
    // =========================================================
    EntityID activePlayerId = CCL::ECS::InvalidEntityID;

    auto playerView = _world->View<TPSPlayerComponent,TransformComponent>();
    for (auto p : playerView) {
        activePlayerId = p;
        break;
    }

    // プレイヤーが見つからなければAIは思考を停止するか、待機する
    if (activePlayerId == CCL::ECS::InvalidEntityID) return;


    // ★ 並列実行 (ForEachParallel) 
    // テンプレート引数とラムダの引数型を【完全に一致】させなければコンパイルエラーになる
    ForEachParallel([this, activePlayerId](BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp,
        const TransformComponent& trans, const BossActionComponent& bossAction, const TimeState& time, const HealthComponent& health) {

        // ★ 各ボスの時計を使用（ヒットストップ中はAIの思考も完全に止まる）
        float dt = time.localDt;

        // ========================================================
        // ★追加: 自身のHP割合をBlackboardに毎フレーム書き込む（同期）
        // ========================================================
        if (health.maxHealth > 0.0f) {
            btComp.blackboard.healthPercentage = (health.currentHealth / health.maxHealth) * 100.0f;
        }


        // ========================================================
        //  フェーズ移行（第2形態への覚醒）の監視
        // ========================================================
        if (btComp.blackboard.currentPhase == 1 && btComp.blackboard.healthPercentage <= btComp.phase2HealthThreshold) {

            btComp.blackboard.currentPhase = 2; // フェーズを2に更新

            // ★ 究極のハック: 読み込むパスを第2形態のものに書き換えるだけ！
            btComp.assetPath = btComp.phase2AssetPath;

            // 現在実行中のノード記憶をリセット（変な状態を引き継がないため）
            btComp.runningNodeId = 0xFFFF;
            btComp.previousRunningNodeId = 0xFFFF;

            CCL_LOG_INFO(LogCategory::AI, "Boss Phase 2 Activated! Loading new Brain: %s", btComp.assetPath.c_str());
        }

        // ========================================================
        // 1. 動的ロード
        // ========================================================
        if (btComp.assetPath != btComp.loadedAssetPath) {
            
            // パスが書き換わった瞬間（または初回起動時）だけロードを実行
            if (!btComp.assetPath.empty()) {
                // ========================================================
                // ★ 修正の要：ポインタが空なら、読み込む前に必ず「箱」を作る！
                // ========================================================
                if (!btComp.sharedAsset) {
                    btComp.sharedAsset = std::make_shared<BTAsset>();
                }

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
        if (btComp.debugNodeStates.size() != nodeCount) {
            btComp.debugNodeStates.resize(nodeCount, BTDebugState::None);
            // 初期値を 0.0f ではなく -1.0f (未実行) に変更
            btComp.nodeTimers.resize(nodeCount, -1.0f);

            // しおり配列の初期化 (-1 は「まだ何も読んでいない」状態)
            btComp.runningNodes.resize(nodeCount, -1);
        }

        // 3. タイマーの減算
        for (size_t i = 0; i < btComp.nodeTimers.size(); ++i) {
            // タイマーがセットされている(0.0fより大きい)場合のみ減らす
            if (btComp.nodeTimers[i] > 0.0f) {
                btComp.nodeTimers[i] -= dt;

                // 0以下になったら 0.0f (完了済み) にピタッと止める
                if (btComp.nodeTimers[i] <= 0.0f) {
                    btComp.nodeTimers[i] = 0.0f;
                }
            }
        }
        

        // デバッグ状態の初期化と、タイマーの進行（クールダウンの消化）
        std::fill(btComp.debugNodeStates.begin(), btComp.debugNodeStates.end(), BTDebugState::None);
   

        // 2. ブラックボードの更新（距離計算）
        const auto* playerTrans = _world->GetComponent<TransformComponent>(activePlayerId);
        if (playerTrans) {
            btComp.blackboard.distanceToTarget = Vector3::Distance(trans.position, playerTrans->position);
        }


        // コマンドIDをリセット
        cmdComp.currentActionId = 0;
        cmdComp.targetPlayerId = activePlayerId;



        // ========================================================
        // 4. ツリーの評価実行
        // ========================================================
        // 前フレームのRunningノードを退避してからリセットする
        btComp.previousRunningNodeId = btComp.runningNodeId;
        btComp.runningNodeId = 0xFFFF; // 評価前にRunning状態をリセット

        // 強制アクションオーバーライド（デバッガー機能）
        if (m_enableActionOverride) {
            ActionID overrideId = g_ActionRegistry[m_selectedOverrideIndex].id;

            if (overrideId >= 2000) {
                // 2000番台(ドローン命令)をDroneFormationTypeのEnum値に自動変換して命令する
                cmdComp.requestFormation = static_cast<DroneFormationType>(overrideId - 2000 + 1);
            }
            else {
                // 1000番台(ボス本体への命令)
                cmdComp.currentActionId = overrideId;
            }
            return; // ★ここで return することで、BTの思考を完全にスキップして命令だけを出し続ける
        }

        if (btComp.sharedAsset && !btComp.sharedAsset->nodes.empty()) {

            BTNodeState treeResult = EvaluateTreeRecursive(0, btComp, cmdComp, trans, bossAction, *btComp.sharedAsset, dt);

            // ========================================================
            // コンボが完全に終わった、または中断された場合、
            // Actionノードの「通過済み記憶(0.0f)」を消去(-1.0f)し、次回のコンボに備える。
            // ※Decorator(クールダウン)のタイマーは消さないようにノードの種類を判定する
            // ========================================================
            if (treeResult == BTNodeState::Success || treeResult == BTNodeState::Failure) {
                for (size_t i = 0; i < btComp.sharedAsset->nodes.size(); ++i) {
                    if (btComp.sharedAsset->nodes[i].type == BTNodeType::Action) {
                        btComp.nodeTimers[i] = -1.0f;
                    }
                }
            }

            // =======================================================
            // ログ出力処理
            // =======================================================
            std::string currentActionName = "Idle"; // デフォルト状態

            if (btComp.lastLogActionId != 0xFFFF) {
                for (int i = 0; i < g_ActionRegistryCount; ++i) {
                    if (g_ActionRegistry[i].id == btComp.lastLogActionId) {
                        currentActionName = g_ActionRegistry[i].name;
                        break;
                    }
                }
            }

            // 前回のアクションから変わった瞬間だけコンソールに出力する
            if (btComp.lastFrameTreeLog != currentActionName) {
                if (!btComp.lastFrameTreeLog.empty()) {
                    CCL_LOG_INFO(LogCategory::AI, "Boss Action Transition: [ %s ] -> [ %s ]",
                        btComp.lastFrameTreeLog.c_str(), currentActionName.c_str());
                }
                btComp.lastFrameTreeLog = currentActionName;
            }
        }


    });
}


// 条件を評価する関数。条件IDに応じて、Blackboardの情報を参照して真偽を返す。
bool BossAISystem::EvaluateCondition(ActionID condId, const BehaviorTreeComponent& btComp)
{
    switch (condId) {
    case AI::C_RangeClose:
        return btComp.blackboard.distanceToTarget < 15.0f;
    case AI::C_RangeMedium:
        return btComp.blackboard.distanceToTarget >= 15.0f && btComp.blackboard.distanceToTarget < 40.0f; 
    case AI::C_IsPhase2:
        return btComp.blackboard.currentPhase >= 2; // 後半戦かどうか
    case AI::C_RangeMostClose:
        return btComp.blackboard.distanceToTarget < 8.0f;
	case AI::C_DamageThresholdExceeded:
		return btComp.blackboard.accumulatedDamage >= btComp.blackboard.evadeDamageThreshold; // 蓄積ダメージが閾値以上か
    default: return false;
    }
}

// ============================================================================
// BossAISystem.cpp : ExecuteAction関数を丸ごと上書き
// ============================================================================
BTNodeState BossAISystem::ExecuteAction(ActionID actionId, BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp,
    const TransformComponent& myTrans, const BossActionComponent& myAction,
    BTNodeID nodeId, float dt)
{
    // 現在評価されたアクションのIDを確実に記憶する
    btComp.lastLogActionId = actionId;

    // ========================================================
    // 1. 汎用・待機アクション (ID: 100 〜 111)
    // ========================================================
    if (actionId >= AI::A_Wait0_5s && actionId <= AI::A_Wait10_0s) {
        if (btComp.nodeTimers[nodeId] == -1.0f) {
            // 配列のインデックス（0〜11）を使って、長大なswitch文を1行に圧縮！
            static const float waitTimes[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
            btComp.nodeTimers[nodeId] = waitTimes[actionId - AI::A_Wait0_5s];
        }
        if (btComp.nodeTimers[nodeId] > 0.0f) return BTNodeState::Running;
        return BTNodeState::Success;
    }


    // ========================================================
    // 2. ボス本体のアクション (ID: 1000番台)
    // ========================================================
    if (actionId >= 1000 && actionId < 2000) {

        // --- 例外処理 ---
        if (actionId == AI::A_Move) {
            if (btComp.nodeTimers[nodeId] == -1.0f) btComp.nodeTimers[nodeId] = 1.0f;
            cmdComp.currentActionId = actionId;
            return BTNodeState::Running; // Moveは自発的には終わらない
        }
        if (actionId == AI::A_ResetDamage) {
            btComp.blackboard.accumulatedDamage = 0.0f;
            return BTNodeState::Success;
        }

        // --- 筋肉連動アクション (攻撃・回避) の汎用処理 ---

        // [完了済み] タイマーが0ならSuccess
        if (btComp.nodeTimers[nodeId] == 0.0f) return BTNodeState::Success;

        // [未実行] -1.0f なら筋肉に発注し、仕様書通りのタイマーをセット
        if (btComp.nodeTimers[nodeId] == -1.0f) {

            // 怯み(Flinch)中は発注できないので待機
            if (myAction.currentState == BossActionState::Flinch) {
                return BTNodeState::Running;
            }

            cmdComp.currentActionId = actionId;

            // 筋肉の仕様書（BossTimings）からタイマーを取得してセット
            if (actionId == AI::A_MeleeAttack) btComp.nodeTimers[nodeId] = BossTimings::Melee_Duration;
            else if (actionId == AI::A_ChargeAttack) btComp.nodeTimers[nodeId] = BossTimings::Charge_Total;
            else if (actionId == AI::A_JumpAttack) btComp.nodeTimers[nodeId] = BossTimings::JumpAttack_Total;
            else if (actionId == AI::A_EvadeBackward) btComp.nodeTimers[nodeId] = BossTimings::Evade_Total;

            return BTNodeState::Running;
        }

        return BTNodeState::Running;
    }

    // ========================================================
    // 3. ドローンに対する指示 (ID: 2000番台)
    // ========================================================
    if (actionId >= 2000 && actionId < 3000) {
        // ★ 究極のハック: 14個あった case 文を数学的に1行に圧縮！
        // ActionID(2000番台) から 2000 を引き、+1 することで
        // DroneFormationType の Enum値 (1〜14) に完璧に変換されます。
        cmdComp.requestFormation = static_cast<DroneFormationType>(actionId - 2000 + 1);
        return BTNodeState::Success;
    }

    return BTNodeState::Failure;
}


// ========================================================================
// ★ カプセル化された再帰評価エンジン（デコレーター＆デバッガー対応版）
// ========================================================================
BTNodeState BossAISystem::EvaluateTreeRecursive(
    BTNodeID nodeId,
    BehaviorTreeComponent& btComp,
    BossCommandComponent& cmdComp,
    const TransformComponent& myTrans,
    const BossActionComponent& myAction,
    const BTAsset& asset,
    float dt)
{
    const BTNode& node = asset.nodes[nodeId];
    BTNodeState result = BTNodeState::Failure;

    switch (node.type) {
    case BTNodeType::Selector: {
        // 専用の配列から「しおり」を取り出す（時間の減算による影響を受けない！）
        int runningIndex = btComp.runningNodes[nodeId];

        if (runningIndex >= 0 && runningIndex < node.children.size()) {
            result = EvaluateTreeRecursive(node.children[runningIndex], btComp, cmdComp, myTrans, myAction, asset, dt);
            if (result == BTNodeState::Running) break;

            if (result == BTNodeState::Success) {
                btComp.runningNodes[nodeId] = -1; // 成功したらしおりを捨てる
                break;
            }
            btComp.runningNodes[nodeId] = -1;     // 失敗したらしおりを捨てる
        }

        result = BTNodeState::Failure;
        for (size_t i = 0; i < node.children.size(); ++i) {
            BTNodeID childId = node.children[i];
            result = EvaluateTreeRecursive(childId, btComp, cmdComp, myTrans, myAction, asset, dt);

            if (result == BTNodeState::Running) {
                btComp.runningNodes[nodeId] = i; // ★修正: 正確なインデックス(int)を保存
                break;
            }
            if (result == BTNodeState::Success) {
                btComp.runningNodes[nodeId] = -1;
                break;
            }
        }
        break;
    }
    case BTNodeType::Sequence: {
        int startIndex = 0;
        // ★修正: 専用の配列から「しおり」を取り出す
        int runningIndex = btComp.runningNodes[nodeId];
        if (runningIndex >= 0 && runningIndex < node.children.size()) {
            startIndex = runningIndex;
        }

        result = BTNodeState::Success;
        for (size_t i = startIndex; i < node.children.size(); ++i) {
            BTNodeID childId = node.children[i];
            result = EvaluateTreeRecursive(childId, btComp, cmdComp, myTrans, myAction, asset, dt);

            if (result == BTNodeState::Running) {
                btComp.runningNodes[nodeId] = i; // ★修正: 正確なインデックス(int)を保存
                break;
            }
            if (result == BTNodeState::Failure) {
                btComp.runningNodes[nodeId] = -1;
                break;
            }
        }

        if (result == BTNodeState::Success) {
            btComp.runningNodes[nodeId] = -1;
        }
        break;
    }
    case BTNodeType::Condition: {
        result = EvaluateCondition(node.actionOrConditionId, btComp) ? BTNodeState::Success : BTNodeState::Failure;
        break;
    }
    case BTNodeType::Action: {
        result = ExecuteAction(node.actionOrConditionId, btComp, cmdComp, myTrans, myAction, nodeId, dt);
        break;
    }
    case BTNodeType::Decorator: {
        int decType = static_cast<int>(node.decoratorType); // キャスト事故防止

        if (decType == 1 && btComp.nodeTimers[nodeId] > 0.0f) {
            result = BTNodeState::Failure;
            break;
        }

        if (!node.children.empty()) {
            BTNodeID childId = node.children[0];

            if (decType == 2) {
                int retryCount = static_cast<int>(node.decoratorParam);
                for (int r = 0; r < retryCount; ++r) {
                    result = EvaluateTreeRecursive(childId, btComp, cmdComp, myTrans, myAction, asset, dt);
                    if (result != BTNodeState::Failure) break;
                }
            }
            else {
                result = EvaluateTreeRecursive(childId, btComp, cmdComp, myTrans, myAction, asset, dt);
            }

            if (decType == 0) {
                if (result == BTNodeState::Success) result = BTNodeState::Failure;
                else if (result == BTNodeState::Failure) result = BTNodeState::Success;
            }
            else if (decType == 1) {
                // ★究極の修正: Success時だけでなく、Running(実行中)の段階でセットする。
                // 実行中は毎フレーム上書きされ、終わった瞬間(または中断された瞬間)からカウントダウンが始まります。
                if (result == BTNodeState::Success) {
                    btComp.nodeTimers[nodeId] = node.decoratorParam;
                }
            }
            // =======================================================
            //  非同期 (Async) のハック
            // =======================================================
            else if (decType == 3) {
                if (result == BTNodeState::Running) {
                    BTNodeID childId = node.children[0];
                    const BTNode& childNode = asset.nodes[childId];

                    // 子が「アクション（筋肉の稼働）」である場合
                    if (childNode.type == BTNodeType::Action) {
                        // 子のタイマーが -1.0f でない ＝ 筋肉への発注（命令）が完了した証拠！
                        if (btComp.nodeTimers[childId] != -1.0f) {
                            result = BTNodeState::Success; // 発注完了！脳を先に進ませる
                        }
                        else {
                            // タイマーが -1.0f のまま ＝ 怯み(Flinch)等で筋肉が命令を聞いてくれていない！
                            // この時は勝手にSuccessにせず、発注できるまでしっかり待機させる。
                            result = BTNodeState::Running;
                        }
                    }
                    else {
                        // Action以外（SequenceなどをAsyncで包んだ場合）は今まで通り無条件で進める
                        result = BTNodeState::Success;
                    }
                }
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
    }
    else if (result == BTNodeState::Failure) {
        dbgState = BTDebugState::Failure;
    }
    else if (result == BTNodeState::Running) {
        dbgState = BTDebugState::Running;
    }

    // エディタのデバッガー向けに結果を保存
    btComp.debugNodeStates[nodeId] = dbgState;

    // すでに深い階層（Actionノードなど）で Running がセットされている場合は、
    // 親ノードのIDで上書きしないように保護する
    if (result == BTNodeState::Running && btComp.runningNodeId == 0xFFFF) {
        btComp.runningNodeId = nodeId;
    }
    return result;
}



// ============================================================================
// システム専用GUIの描画と3D空間デバッグテキスト
// ============================================================================
void BossAISystem::OnGui() {
    if (ImGui::CollapsingHeader("Boss AI Debugger", ImGuiTreeNodeFlags_DefaultOpen)) {

        // ★修正: デバッグUIのコントロールパネルを追加
        ImGui::Checkbox("Show 3D Floating Text", &m_showFloatingText);
        ImGui::Indent();
        ImGui::SliderFloat("Font Size", &m_debugFontSize, 10.0f, 100.0f, "%.1f px");
        ImGui::Checkbox("Use Depth Scaling (遠近法)", &m_useDepthScaling);
        ImGui::Checkbox("Show Text Background (座布団)", &m_showTextBackground);
        ImGui::Unindent();
        ImGui::Separator();

        // ========================================================
        //  アクション・オーバーライドUI
        // ========================================================
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Action Override Debugger");
        ImGui::Checkbox("Enable Override (BT思考を強制停止)", &m_enableActionOverride);
        if (m_enableActionOverride) {
            if (ImGui::BeginCombo("Force Action", g_ActionRegistry[m_selectedOverrideIndex].name)) {
                for (int i = 0; i < g_ActionRegistryCount; ++i) {
                    bool isSelected = (m_selectedOverrideIndex == i);
                    if (ImGui::Selectable(g_ActionRegistry[i].name, isSelected)) {
                        m_selectedOverrideIndex = i;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        ImGui::Separator();

        // ========================================================
        // ★新規追加: プレイヤーの強制ワープ・デバッグUI
        // ========================================================
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Player Distance Debugger");

        // プレイヤーの検索
        CCL::ECS::EntityID activePlayerId = CCL::ECS::InvalidEntityID;
        auto playerView = _world->View<TPSPlayerComponent, TransformComponent>();
        for (auto p : playerView) { activePlayerId = p; break; }

        if (activePlayerId != CCL::ECS::InvalidEntityID) {
            auto* playerTrans = _world->GetComponent<TransformComponent>(activePlayerId);
            if (playerTrans) {
                // DragFloat3 で位置を直接編集できるようにする
                float pos[3] = { playerTrans->position.x, playerTrans->position.y, playerTrans->position.z };

                // 0.2f ずつ動かす。Shiftを押しながらドラッグで高速移動可能
                if (ImGui::DragFloat3("Player Pos (Warp)", pos, 0.2f)) {
                    playerTrans->position = DirectX::SimpleMath::Vector3(pos[0], pos[1], pos[2]);

                    // ★最重要: ECSの変更フラグと、Jolt物理エンジンへの「ワープ通知」を立てる
                    // これを行わないと、物理エンジンに座標を引き戻されたり、超音速で吹っ飛んだりする
                    playerTrans->isTeleported = true;
                    playerTrans->isDirty = true;
                }
            }
        }
        else {
            ImGui::TextDisabled("TPS Player not found in scene.");
        }
        ImGui::Separator();

        auto view = _world->View<BehaviorTreeComponent, TransformComponent>();
        for (auto e : view) {
            auto* btComp = _world->GetComponent<BehaviorTreeComponent>(e);
            auto* trans = _world->GetComponent<TransformComponent>(e);

            ImGui::Text("Boss Entity ID: %d", e);
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Current Action: %s", btComp->lastFrameTreeLog.c_str());

            if (ImGui::TreeNode("Blackboard Variables")) {
                ImGui::Text("Distance To Target: %.2fm", btComp->blackboard.distanceToTarget);
                ImGui::Text("Current Phase: %d", btComp->blackboard.currentPhase);
                ImGui::Text("HP: %.1f%%", btComp->blackboard.healthPercentage);
                ImGui::TreePop();
            }
            ImGui::Spacing();

            // -----------------------------------------------------
            // 2. 3D空間へのForegroundDrawListテキスト描画 (World to Screen)
            // -----------------------------------------------------
            if (m_showFloatingText) {
                const auto* camera = _world->GetResource<Camera*>();
                if (camera) {

                    Matrix viewProj = camera->GetView() * camera->GetProjection();

                    Vector3 worldPos = trans->position;
                    worldPos.y += 3.0f;

                    Vector3 ndcPos = Vector3::Transform(worldPos, viewProj);

                    if (ndcPos.z > 0.0f && ndcPos.z < 1.0f) {

                        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
                        float pixelX = (ndcPos.x + 1.0f) * 0.5f * screenSize.x;
                        float pixelY = (1.0f - ndcPos.y) * 0.5f * screenSize.y;

                        ImDrawList* drawList = ImGui::GetForegroundDrawList();
                        ImFont* font = ImGui::GetFont(); // 現在のフォントを取得

                        // ★追加: 基準のフォントサイズ
                        float currentFontSize = m_debugFontSize;

                        // ★追加: 遠近法（カメラから遠いほど文字が小さくなる）
                        if (m_useDepthScaling) {
                            // ndcPos.z は 0.0(超近い) ～ 1.0(超遠い)
                            // 遠いときは最大で 0.3倍 のサイズまで小さくする
                            float scale = 1.0f - (ndcPos.z * 0.7f);
                            currentFontSize *= scale;
                        }

                        char line1[128];
                        snprintf(line1, sizeof(line1), "[Action: %s]", btComp->lastFrameTreeLog.c_str());
                        char line2[128];
                        snprintf(line2, sizeof(line2), "Dist: %.1fm", btComp->blackboard.distanceToTarget);

                        // ★修正: フォントサイズを指定して「文字が占めるピクセル幅」を正確に計算
                        ImVec2 size1 = font->CalcTextSizeA(currentFontSize, FLT_MAX, 0.0f, line1);
                        ImVec2 size2 = font->CalcTextSizeA(currentFontSize, FLT_MAX, 0.0f, line2);

                        // 基準位置から文字幅の半分を引く（完全な中央揃え）
                        float drawX1 = pixelX - size1.x * 0.5f;
                        float drawY1 = pixelY - (size1.y + size2.y) * 0.5f;
                        float drawX2 = pixelX - size2.x * 0.5f;
                        float drawY2 = drawY1 + size1.y;

                        // =======================================================
                        // ★追加: 背景の半透明な黒い板（座布団）を描画
                        // =======================================================
                        if (m_showTextBackground) {
                            float maxWidth = (std::max)(size1.x, size2.x);
                            float totalHeight = size1.y + size2.y;

                            // 文字の周囲に少しだけ余白（Padding）をもたせる
                            float paddingX = currentFontSize * 0.4f;
                            float paddingY = currentFontSize * 0.2f;

                            ImVec2 bgMin(pixelX - maxWidth * 0.5f - paddingX, drawY1 - paddingY);
                            ImVec2 bgMax(pixelX + maxWidth * 0.5f + paddingX, drawY1 + totalHeight + paddingY);

                            // 黒色(0,0,0)で、透明度(160/255)の角丸(6.0f)四角形
                            drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 160), 6.0f);
                        }

                        // =======================================================
                        // フォントサイズ対応版のアウトライン付き描画ラムダ
                        // =======================================================
                        ImU32 outlineCol = IM_COL32(0, 0, 0, 255);
                        ImU32 textCol1 = IM_COL32(255, 255, 0, 255);
                        ImU32 textCol2 = IM_COL32(255, 255, 255, 255);

                        auto DrawOutlinedText = [&](float x, float y, ImU32 col, const char* text) {
                            // サイズを指定できる AddText のオーバーロードを使用
                            drawList->AddText(font, currentFontSize, ImVec2(x - 1, y), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x + 1, y), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x, y - 1), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x, y + 1), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x - 1, y - 1), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x + 1, y - 1), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x - 1, y + 1), outlineCol, text);
                            drawList->AddText(font, currentFontSize, ImVec2(x + 1, y + 1), outlineCol, text);
                            // メイン文字
                            drawList->AddText(font, currentFontSize, ImVec2(x, y), col, text);
                            };

                        DrawOutlinedText(drawX1, drawY1, textCol1, line1);
                        DrawOutlinedText(drawX2, drawY2, textCol2, line2);
                    }
                }
            }
        }
    }
}



// 規約に基づき末尾でシステム自動登録。
// 実行タイミングは「L02_Update (AI、移動、弾の更新など並列化の主戦場)」を指定。
// ※プロジェクト側に REGISTER_LOGIC_SYSTEM マクロが定義されている前提
REGISTER_LOGIC_SYSTEM(BossAISystem, Priority::LogicStage::L02_Update);