#pragma once
//4/15作製　桃田
// --- PlayerFPSMoveSystem.h ---
#include "ECS/System/CCL_System.h"

class TransformComponent;
class FPSPlayerComponent;
class TPSPlayerStateSystem;
class StaminaComponent;
class ModifierStatusComponent;

class FPSPlayerMoveSystem
    : public CCL::ECS::IfSystem<FPSPlayerMoveSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<FPSPlayerComponent>,
    CCL::ECS::Write<StaminaComponent>,
    CCL::ECS::Read<ModifierStatusComponent>>
{
public:
    FPSPlayerMoveSystem() : IfSystem("FPSPlayerMoveSystem") {}
    void Update(float dt) override;
};