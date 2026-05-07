#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/AI/NavAI/AIComponents.h"
#include "Engine/GamePlay/AI/Navigation/NavAgentComponent.h"

class AIMovementControlSystem : public CCL::ECS::IfSystem<AIMovementControlSystem,
    CCL::ECS::Read<AIStateComponent>,
    CCL::ECS::Write<NavAgentComponent>> {
public:
    AIMovementControlSystem() : IfSystem("AIMovementControlSystem") {}
    virtual ~AIMovementControlSystem() = default;

    void Update(float dt) override;
};