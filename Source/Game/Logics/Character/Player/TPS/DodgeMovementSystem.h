#pragma once
#include "ECS/System/CCL_System.h"

#include "../PlayerStateComponent.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

// 前方宣言
struct TPSPlayerComponent;
struct TransformComponent;


/**
 * IsDashingTag を持つプレイヤーに対して、超高速な移動を適用するシステム
 */
class DodgeMovementSystem
    : public CCL::ECS::IfSystem<DodgeMovementSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<TPSPlayerComponent>,
    CCL::ECS::Write<TPSPlayerStateComponent>,
    CCL::ECS::Write<PlayerStateTag::IsDashingTag>,
    CCL::ECS::Read<TimeState>> 
{
public:
    DodgeMovementSystem() : IfSystem("DodgeMovementSystem") {}

    void Update(float dt) override;
};