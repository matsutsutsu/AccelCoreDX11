#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "Game/Logic/AI/BehaviorTree/Data/BehaviorTreeComponents.h" 
#include "BossActionComponent.h"
#include "Game/Logic/Character/CharacterMovementInputComponent.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

// ===================================================================================
// ファイル名: BossActionSystem.h
// 役割: AIの出力(BossCommand)を読み取り、ボスの物理移動とアニメーションに変換する実行部隊
// ===================================================================================

class BossActionSystem : public CCL::ECS::IfSystem<
    BossActionSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<AnimParametersComponent>,
    CCL::ECS::Write<BossActionComponent>,
    CCL::ECS::Write<CharacterMovementInputComponent>, 
    CCL::ECS::Read<BossCommandComponent>,
    CCL::ECS::Read<TimeState> 
>
{
public:
    BossActionSystem() : IfSystem("BossActionSystem") {}
    virtual ~BossActionSystem() override = default;

    virtual void Update(float dt) override;

private:
    // =========================================================
    // 視認性を高めるための、アクションごとのヘルパー関数群
    // =========================================================

    /**
     * @brief AIからの新しい命令を受け取り、状態を更新する
     */
    void AcceptNewCommand(BossActionComponent& bossDef, const BossCommandComponent& cmd, TransformComponent& trans);

    /**
     * @brief プレイヤーへ向かって歩く処理 (Move)
     */
    void ProcessMove(BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, TransformComponent& trans, const BossCommandComponent& cmd, float dt, float& outCurrentSpeed);

    /**
      * @brief 記憶した方向へ突進する処理 (Charge)
      */
    void ProcessCharge(BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, TransformComponent& trans, const BossCommandComponent& cmd, float dt, float& outTargetSpeed);

    /**
     * @brief プレイヤーの位置にジャンプして着地する処理 (Jump Attack)
     */
    void ProcessJumpAttack(BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, TransformComponent& trans, float dt, float& outCurrentSpeed);

    /**
     * @brief バックステップで距離を取る処理 (Evade)
     */
    void ProcessEvade(BossActionComponent& bossDef, float dt, float& outTargetSpeed);

    // =========================================================
    //  待機中にプレイヤーを滑らかに注視する処理 (Idle)
    // =========================================================
    void ProcessIdle(BossActionComponent& bossDef, TransformComponent& trans, const BossCommandComponent& cmd, float dt);


};