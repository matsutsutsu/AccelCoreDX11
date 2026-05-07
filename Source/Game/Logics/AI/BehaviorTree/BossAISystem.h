#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeComponents.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeData.h"

/**
 * @file BossAISystem.h
 * @brief ボスAIの思考評価を実行するECSシステムの定義
 * * 役割：
 * BehaviorTreeComponent を持つ全Entityを対象に、毎フレームの意思決定を行う。
 * * 特徴：
 * IfSystem を継承し、Read/Write 権限を明示することで
 * TaskScheduler による自動並列化（マルチスレッド）に対応している。
 */

 // IfSystemのテンプレート引数で要求コンポーネントを厳密に定義
class BossAISystem : public CCL::ECS::IfSystem<
    BossAISystem,
    CCL::ECS::Write<BehaviorTreeComponent>,
    CCL::ECS::Write<BossCommandComponent>
>
{
public:
    BossAISystem();
    virtual ~BossAISystem() override = default;

    virtual void Update(float dt) override;

private:

    /*
    * @param 再起処理で評価する
    */
    BTNodeState EvaluateTreeRecursive(
        BTNodeID nodeId,
        BehaviorTreeComponent& btComp,
        BossCommandComponent& cmdComp,
        const BTAsset& asset,
        float dt
    );

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
    BTNodeState ExecuteAction(ActionID actionId, BehaviorTreeComponent& btComp, BossCommandComponent& cmdComp, BTNodeID nodeId, float dt);


};