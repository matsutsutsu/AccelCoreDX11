#pragma once
#include "ECS/System/CCL_System.h"

class ModifierComponent;
class ModifierStatusComponent;

/**
プレイヤーの状態に基づいてカメラのFOV補間などを行うシステム
 */
class PlayerModifierSystem
    : public CCL::ECS::IfSystem<PlayerModifierSystem,
    CCL::ECS::Write<ModifierComponent>,
    CCL::ECS::Write<ModifierStatusComponent>>
{
public:
    PlayerModifierSystem() : IfSystem("ModifierSystem") {}
    virtual ~PlayerModifierSystem() = default;

    void Update(float dt) override;
};