#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"
#include "../PlayerStateComponent.h"

// 前方宣言
struct TPSPlayerComponent;
struct TransformComponent;

// TPSPlayerChainAttackSystem.cpp
class TPSPlayerChainAttackSystem : public CCL::ECS::IfSystem<TPSPlayerChainAttackSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<TPSPlayerComponent>,
    CCL::ECS::Write<TPSPlayerStateComponent>,
    CCL::ECS::Read<PlayerStateTag::IsChainAttackTag>,
    CCL::ECS::Read<TimeState>> // ★追加
{
public:
    TPSPlayerChainAttackSystem() : IfSystem("TPSPlayerChainAttackSystem") {}

    void Update(float dt) override;
};