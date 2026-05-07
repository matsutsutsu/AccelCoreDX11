/**
 * @file DroneFormationSystem.h
 * @brief ボスの指示に基づいて全ドローンの目標座標（TargetPosition）を計算するシステム
 */
#pragma once
#include "ECS/System/CCL_System.h"
#include "DroneComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics//AI/BehaviorTree/Data/BehaviorTreeComponents.h"

 /**
  * @class DroneFormationSystem
  * @brief ドローンのフォーメーション制御システム
  * * @warning このシステムは「目標座標の計算」のみを行う。実際のTransform更新は行わない。
  */
class DroneFormationSystem : public CCL::ECS::IfSystem<
    DroneFormationSystem,
    CCL::ECS::Write<DroneComponent> // ドローンの状態と目標座標を書き換える
>
{
public:
    DroneFormationSystem() : IfSystem("DroneFormationSystem") {}
    virtual ~DroneFormationSystem() override = default;


    virtual void Update(float dt) override;

private:
    // システムローカルの経過時間
    float m_currentTime = 0.0f;

    // =========================================================
    // 計算ヘルパー関数群（インライン展開によりオーバーヘッドゼロ）
    // =========================================================

    /**
     * @brief ボスからの命令が変更された瞬間の状態遷移処理
     */
    inline void HandleStateTransition(DroneComponent& drone, const BossCommandComponent& bossCmd);

    /**
     * @brief 円形フォーメーションの目標座標を計算する
     */
    inline void CalculateOrbitCircle(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime);

    /**
     * @brief 順番攻撃フォーメーションの目標座標と状態を計算する
     */
    inline void CalculateSequentialAttack(DroneComponent& drone, const BossCommandComponent& bossCmd, float dt);

    /**
	 * @brief 死の輪フォーメーションの目標座標を計算する
    */
    inline void CalculateDeathRing(DroneComponent& drone, const BossCommandComponent& bossCmd, float currentTime);

    /**
     * @brief 絶対防衛陣形の目標座標を計算する
     */
    inline void CalculateAegisShield(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd);
};