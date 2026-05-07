#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/AI/NavAI/AIComponents.h"
#include "Engine/GamePlay/AI/Navigation/NavAgentComponent.h"
#include "Game/Logics/AI/NavAI/Events/AISoundEvent.h"
#include <vector>
#include <mutex>

/**
 * @brief 空間の音イベントを監視し、AIの聴覚判定を行うシステム。
 */
class AIHearingSystem : public CCL::ECS::IfSystem<AIHearingSystem,
                               CCL::ECS::Read<TransformComponent>,
                               CCL::ECS::Read<AIPerceptionComponent>,
                               CCL::ECS::Write<AIMemoryComponent>,
                               CCL::ECS::Write<AIStateComponent>,
                               CCL::ECS::Write<NavAgentComponent>> {
private:
    std::vector<AISoundEvent> m_frameEvents;
    std::mutex                m_eventMutex;

public:
    AIHearingSystem(const std::string& name = "AIHearingSystem") : IfSystem(name) {}
    virtual ~AIHearingSystem() = default;

    void Initialize() override;

    void Update(float dt) override;
};