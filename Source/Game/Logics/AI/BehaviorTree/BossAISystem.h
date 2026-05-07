#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logic/AI/BehaviorTree/Data/BehaviorTreeComponents.h"
#include "Game/Logic/AI/BehaviorTree/Data/BehaviorTreeData.h"
#include "Game/Logic/AI/BehaviorTree/BossActionComponent.h"      
#include "Engine/GamePlay/Transform/TransformComponent.h" 
#include "Engine/GamePlay/Core/Time/TimeState.h"
#include "Game/Logic/Combat/HealthComponent.h"

/**
 * @file BossAISystem.h
 * @brief ボスAIの思考評価を実行するECSシステムの定義
 * * 役割：
 * BehaviorTreeComponent を持つ全Entityを対象に、毎フレームの意思決定を行う。
 * * 特徴：
 * IfSystem を継承し、Read/Write 権限を明示することで
 * TaskScheduler による自動並列化（マルチスレッド）に対応している。
 */


class BossAISystem : public CCL::ECS::IfSystem<
    BossAISystem,
    CCL::ECS::Write<BehaviorTreeComponent>,
    CCL::ECS::Write<BossCommandComponent>,
    CCL::ECS::Read<TransformComponent>,  // ★自分の位置を知るため
    CCL::ECS::Read<BossActionComponent>,
    CCL::ECS::Read<TimeState>,
    CCL::ECS::Read<HealthComponent> // HPを読み取る権限
>
{
public:
    BossAISystem();
    virtual ~BossAISystem() override = default;

    virtual void Update(float dt) override;

    virtual void OnGui() override; // GUI描画用関数

private:
    bool m_showFloatingText = true; // 3Dテキスト表示のON/OFFフラグ

    // 視認性向上のためのパラメータ群
    float m_debugFontSize = 72.0f;    // 文字の基本サイズ
    bool m_useDepthScaling = true;    // 遠近法（遠いと文字が小さくなる）を適用するか
    bool m_showTextBackground = true; // 文字の背景に黒い半透明の板（座布団）を敷くか


    // アクション強制デバッグ用の変数
    bool m_enableActionOverride = false;
    int  m_selectedOverrideIndex = 0;

    /*
    * @param 再起処理で評価する
    */
    BTNodeState EvaluateTreeRecursive(BTNodeID nodeId, 
        BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp,
        const TransformComponent& myTrans, const BossActionComponent& myAction,
        const BTAsset& asset, float dt);

    /*
    * @brief 条件ノードを評価する
    * @param condId 評価する条件のID
    * @param btComp 現在のエンティティが持つAI状態（Blackboardや実行中ノード情報）
    * @return 条件が成立する場合はtrue、そうでない場合はfalse
    */
    bool EvaluateCondition(ActionID condId, const BehaviorTreeComponent& btComp);

    /*
	* @brief 行動ノードを評価する
	* @param actionId 評価する行動のID
	* @param btComp  現在のエンティティが持つAI状態（Blackboardや実行中ノード情報）
	* @param cmdComp 評価結果（行動要求）を書き込むための出力用コンポーネント
    * @return 行動の評価結果（Success, Failure, Running）
    * * @note 【設計上の注意点】
    * この関数は純粋な「評価関数」として設計されている。
	* 物理演算やアニメーションのトリガーなどの副作用は、cmdCompへの書き込みに限定されるべきである。
    * タイマー機能（Wait）を利用するため、nodeId と dt を追加
    */
    BTNodeState ExecuteAction(ActionID actionId, BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp,
        const TransformComponent& myTrans, const BossActionComponent& myAction,
        BTNodeID nodeId, float dt);


};