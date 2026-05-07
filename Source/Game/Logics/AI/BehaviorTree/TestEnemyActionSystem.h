/**
 * @file TestEnemyActionSystem.h
 * @brief AIの出力(BossCommand)を物理移動とアニメーションに変換するシステム
 */
#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeComponents.h" // BossCommandComponent用
#include "TestEnemyActionComponent.h"

 // IfSystemで、必要なコンポーネントが全て揃っているEntityだけを対象にする
class TestEnemyActionSystem : public CCL::ECS::IfSystem<
    TestEnemyActionSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<AnimParametersComponent>,
    CCL::ECS::Write<TestEnemyActionComponent>,
    CCL::ECS::Read<BossCommandComponent> // ★AIの出力（伝言板）を読む
>
{
public:
    TestEnemyActionSystem() : IfSystem("TestEnemyActionSystem") {}
    virtual ~TestEnemyActionSystem() override = default;

    virtual void Update(float dt) override;
};