// PlayerHUDSystem.h
#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logic/Combat/StaminaComponent.h"
#include "Game/Logic/Character/Player/TPS/TPSPlayerComponent.h"

// 引数なしのすっきりしたコンストラクタになる
class PlayerHUDSystem : public CCL::ECS::IfSystem<PlayerHUDSystem,
    CCL::ECS::Read<TPSPlayerComponent>,
    CCL::ECS::Read<StaminaComponent>> 
{
public:
    PlayerHUDSystem() : IfSystem("PlayerHUDSystem") {}
    void Update(float dt) override;

};