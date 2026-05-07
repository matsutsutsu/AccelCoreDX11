#pragma once
#include "ECS/System/CCL_System.h"

#include "../PlayerStateComponent.h"

// 前方宣言
struct TPSPlayerComponent;
struct TransformComponent;

// TPSPlayerLockOnSystem.cpp
class TPSPlayerTargetingSystem : public CCL::ECS::IfSystem<TPSPlayerTargetingSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<TPSPlayerComponent>,
    CCL::ECS::Write<TPSPlayerStateComponent>,
    CCL::ECS::Write<PlayerStateTag::IsLockOnTag>>
{
public:
    TPSPlayerTargetingSystem() : IfSystem("TPSPlayerTargetingSystem") {}

    void Update(float dt) override;
};