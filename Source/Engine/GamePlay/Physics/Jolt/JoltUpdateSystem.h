#pragma once
#include <Jolt/Jolt.h>
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"

// ===================================================================================
// ファイル名: JoltUpdateSystem.h
// 役割:
// 物理シミュレーションの進行(Step)と、結果の受信(Pull)を担う中核システム群。
//
// 【アーキテクチャ仕様】
// 1. JoltStepSystem
//    - 実行フェーズ: Physics
//    - JoltPhysicsManager::Step()
//    を呼び出し、全CPUコアで並列に衝突・落下計算を実行する。
// 2. JoltPullSystem
//    - 実行フェーズ: Collision (または PostPhysics)
//    - 計算が終わったJolt剛体の最新座標を読み取り、ECSの Transform に書き戻す。
//
// 【使い方・ルール】
// - PullSystem では IsActive()
// をチェックし、スリープ中の剛体の座標は同期しない（超最適化）。
// - このフェーズが終わった後、描画システムが最新の Transform
// を使って画面を描画する。
// ===================================================================================


// 1. 物理の時間を進めるシステム
class JoltStepSystem : public CCL::ECS::IfSystem<JoltStepSystem> {
  public:
    JoltStepSystem() : IfSystem("JoltStepSystem") {}
    void Update(float dt) override;
};

// 2. 計算結果をECSのTransformに同期するシステム
class JoltPullSystem : public CCL::ECS::IfSystem<JoltPullSystem,
                           CCL::ECS::Write<TransformComponent>,
                           CCL::ECS::Read<JoltHandleComponent>> {
  public:
    JoltPullSystem() : IfSystem("JoltPullSystem") {}
    void Update(float dt) override;
};