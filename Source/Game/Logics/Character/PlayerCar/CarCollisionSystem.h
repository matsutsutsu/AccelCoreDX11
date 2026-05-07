#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Physics/Collision/JoltCollisionEvent.h"
#include <vector>
#include <mutex>

class CarCollisionSystem : public CCL::ECS::IfSystem<CarCollisionSystem> {
private:
    std::vector<JoltCollisionEvent> m_events;
    std::mutex m_mutex;

    // イベント購読のチケット
    CCL::ECS::Core::EventSubscriptionID m_subID = 0;

public:
    CarCollisionSystem() : IfSystem("CarCollisionSystem") {}
    virtual ~CarCollisionSystem();

    // ★システム初期化時に呼ばれる
    void Initialize() override;

    void Update(float dt) override;

private:
    // EventBusから非同期に呼ばれるコールバック
    void OnCollision(const JoltCollisionEvent& ev);
};