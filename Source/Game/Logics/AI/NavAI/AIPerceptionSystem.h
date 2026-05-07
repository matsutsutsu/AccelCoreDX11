#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/AI/NavAI/AIComponents.h"
#include "Engine/GamePlay/AI/Navigation/NavAgentComponent.h"


// 視覚システム：トランスフォームと知覚設定を読み取り、記憶・状態・ナビを書き換える
class AIPerceptionSystem : public CCL::ECS::IfSystem<AIPerceptionSystem,
    CCL::ECS::Read<TransformComponent>,
    CCL::ECS::Read<AIPerceptionComponent>,
    CCL::ECS::Write<AIMemoryComponent>, 
    CCL::ECS::Write<AIStateComponent>,
    CCL::ECS::Write<NavAgentComponent>> {
public:
    AIPerceptionSystem(const std::string& name = "AIPerceptionSystem") : IfSystem(name) {}
    virtual ~AIPerceptionSystem() = default;

    void Update(float dt) override;
};
