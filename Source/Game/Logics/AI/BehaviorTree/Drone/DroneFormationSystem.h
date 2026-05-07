/**
 * @file DroneFormationSystem.h
 * @brief ボスの指示に基づいて全ドローンの目標座標（TargetPosition）を計算するシステム
 */
#pragma once
#include "ECS/System/CCL_System.h"
#include "DroneComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logic/AI/BehaviorTree/Data/BehaviorTreeComponents.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

 /**
  * @class DroneFormationSystem
  * @brief ドローンのフォーメーション制御システム
  * * @warning このシステムは「目標座標の計算」のみを行う。実際のTransform更新は行わない。
  */
class DroneFormationSystem : public CCL::ECS::IfSystem<
    DroneFormationSystem,
    CCL::ECS::Write<DroneComponent>,
    CCL::ECS::Read<TimeState> 
>
{
public:
    DroneFormationSystem() : IfSystem("DroneFormationSystem") 
    {    
		hasGui = true; // GUIを持つことを宣言
    }
    virtual ~DroneFormationSystem() override = default;

    virtual void Update(float dt) override;
    virtual void OnGui() override;

private:
    // システムローカルの経過時間
    float m_currentTime = 0.0f;

    // GUIで操作するためのパラメータ
    int m_guiDroneCount = 12; // 生成するドローンの数
    float m_guiOrbitRadius = 8.0f; // 生成時の旋回半径
    float m_guiMoveSpeed = 40.0f; // 生成時の速度

    // =========================================================
    // 計算ヘルパー関数群（インライン展開によりオーバーヘッドゼロ）
    // =========================================================

    /**
     * @brief ボスからの命令が変更された瞬間の状態遷移処理
     */
    inline void HandleStateTransition(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd);

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

    /**
     * @brief 上空待機フォーメーションの目標座標を計算する
	 */
    inline void CalculateHighOrbit(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime);
    /**
     * @brief 降下展開フォーメーションの目標座標を計算する
     */
    inline void CalculateLowOrbit(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime);
    /**
     * @brief 拡散静止フォーメーションの目標座標を計算する
     */
	inline void CalculateSpreadLockOn(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime);

    /**
     * @brief フィボナッチ球面を用いた全方位密着バリアの目標座標を計算する
     */
    inline void CalculateCloseGuard(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime);

    /**
    * @brief バリア状態から収縮し、全方位に拡散攻撃を行う (Barrier Burst)
    */
    inline void CalculateBarrierBurst(DroneComponent& drone, const TransformComponent& bossTrans, float dt, float currentTime); // ★ currentTime を追加

    /**
     * @brief ボスとプレイヤーを結ぶ直線上に、横移動を制限する2列の隊列を作る
     */
    inline void CalculateChargeTunnel(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd);


    // サイクロンバーストの計算関数
    inline void CalculateCycloneBurst(DroneComponent& drone, const TransformComponent& bossTrans, float dt, float currentTime);

};