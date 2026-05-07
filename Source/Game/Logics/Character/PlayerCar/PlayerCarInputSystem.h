#pragma once
#include "ECS/System/CCL_System.h"
#include "PlayerCarComponent.h"


class PlayerCarInputSystem
    : public CCL::ECS::IfSystem<PlayerCarInputSystem, CCL::ECS::Write<PlayerCarComponent>> {
public:
    PlayerCarInputSystem();
    virtual ~PlayerCarInputSystem() = default;

    void Update(float dt) override;
};