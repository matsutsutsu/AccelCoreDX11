#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Utils/TimerComponent.h"

class TimerSystem : public CCL::ECS::IfSystem<TimerSystem, CCL::ECS::Write<TimerComponent>> {
  public:
    TimerSystem();
    virtual ~TimerSystem() = default;

    void Update(float dt) override;
};