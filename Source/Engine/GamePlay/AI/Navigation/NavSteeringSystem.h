#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/AI/Navigation/NavAgentComponent.h"
#include "Game/Logics/Character/CharacterMovementInputComponent.h"

// 経路(waypoints)に従って、物理エンジン(Jolt)経由でAIを動かすシステム
class NavSteeringSystem : public CCL::ECS::IfSystem<NavSteeringSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<NavAgentComponent>,
    CCL::ECS::Write<CharacterMovementInputComponent>> {
public:
    NavSteeringSystem() : IfSystem("NavSteeringSystem") {}
    virtual ~NavSteeringSystem() = default;

    void Update(float dt) override;
};