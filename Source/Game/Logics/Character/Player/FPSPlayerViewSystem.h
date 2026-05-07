#pragma once
#include "ECS/System/CCL_System.h"


class FPSPlayerComponent;
class FPSPlayerViewComponent;
class StaminaComponent;
class ModifierStatusComponent;


/**
プレイヤーの状態に基づいてカメラのFOV補間などを行うシステム
 */
class FPSPlayerViewSystem
    : public CCL::ECS::IfSystem<FPSPlayerViewSystem,
    CCL::ECS::Read<FPSPlayerComponent>,
    CCL::ECS::Read<StaminaComponent>,
    CCL::ECS::Read<ModifierStatusComponent>,
    CCL::ECS::Write<FPSPlayerViewComponent>> // currentFOVの書き込みがあるため
{
public:
    FPSPlayerViewSystem() : IfSystem("FPSPlayerViewSystem") {}
    virtual ~FPSPlayerViewSystem() = default;

    void Update(float dt) override;

};