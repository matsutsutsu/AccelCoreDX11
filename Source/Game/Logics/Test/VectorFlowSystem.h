#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "VectorFlowComponent.h"
#include <cmath>

class VectorFlowSystem : public CCL::ECS::IfSystem<VectorFlowSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<VectorFlowComponent>>
{
public:
    VectorFlowSystem() : IfSystem("VectorFlowSystem") {}

    void Update(float dt) override;
};