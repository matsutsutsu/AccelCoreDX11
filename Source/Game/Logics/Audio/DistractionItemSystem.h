#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Physics/Collision/JoltCollisionEvent.h"
#include <vector>
#include <mutex>

class DistractionItemSystem : public CCL::ECS::IfSystem<DistractionItemSystem> {
private:
    std::vector<JoltCollisionEvent> m_frameEvents;
    std::mutex                      m_mutex;


    void CheckAndTriggerSound(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& relVel);

public:
    DistractionItemSystem() : IfSystem("DistractionItemSystem") {}
    virtual ~DistractionItemSystem() = default;

    void Initialize() override;
    void Update(float dt) override;
};