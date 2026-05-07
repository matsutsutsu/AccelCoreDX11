#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "VoxelOceanComponent.h"
#include <cmath>

class VoxelOceanSystem : public CCL::ECS::IfSystem<VoxelOceanSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<VoxelOceanComponent>> {
public:
    VoxelOceanSystem() : IfSystem("VoxelOceanSystem") {}

    void Update(float dt) override;
};