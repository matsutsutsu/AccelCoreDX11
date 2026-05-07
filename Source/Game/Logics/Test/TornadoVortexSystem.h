#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "TornadoVortexComponent.h"
#include <cmath>

class TornadoVortexSystem : public CCL::ECS::IfSystem<TornadoVortexSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<TornadoVortexComponent>>
{
public:
    TornadoVortexSystem() : IfSystem("TornadoVortexSystem") {}

    void Update(float dt) override;
};