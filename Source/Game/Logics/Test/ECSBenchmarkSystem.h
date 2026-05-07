#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "OrbitBenchmarkComponent.h"
#include <cmath>

class ECSBenchmarkSystem : public CCL::ECS::IfSystem<ECSBenchmarkSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<OrbitBenchmarkComponent>> {
public:
    ECSBenchmarkSystem() : IfSystem("ECSBenchmarkSystem") {}

    void Update(float dt) override;
};

