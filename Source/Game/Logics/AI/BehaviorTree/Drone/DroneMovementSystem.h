/**
 * @file DroneMovementSystem.h
 * @brief 振付師が決定した目標座標へ向かってドローンを物理的に移動・回転させるシステム
 */
#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/AI/BehaviorTree/Drone/DroneComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

class DroneMovementSystem : public CCL::ECS::IfSystem<
    DroneMovementSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<DroneComponent> // 目標座標を読むだけ
>
{
public:
    DroneMovementSystem() : IfSystem("DroneMovementSystem") {}
    virtual ~DroneMovementSystem() override = default;

    virtual void Update(float dt) override;
};