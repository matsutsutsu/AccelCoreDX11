#pragma once
#include "ECS/System/CCL_System.h"
#include "StaminaComponent.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

class ModifierStatusComponent;

/**
全てのStaminaComponentの回復・状態更新を行うシステム
 */
class StaminaSystem : 
    public CCL::ECS::IfSystem <StaminaSystem,
    CCL::ECS::Write<StaminaComponent>,
    CCL::ECS::Read<ModifierStatusComponent>,
    CCL::ECS::Read<TimeState>> // ★追加
{
public:
    StaminaSystem() : IfSystem("StaminaSystem") {}
    // 毎フレーム実行
    void Update(float dt) override;
};