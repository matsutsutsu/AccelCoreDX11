#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <mutex>
#include <vector>
#include "Engine/GamePlay/Physics/Collision/JoltCollisionEvent.h"

// Joltの物理スレッドから衝突報告を受け取る「窓口」
class JoltContactListener : public JPH::ContactListener {
public:
    // Joltのワーカースレッドから並列で呼ばれるコールバック（※メインスレッドではない！）
    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                const JPH::ContactManifold& inManifold,
                                JPH::ContactSettings& ioSettings) override;

    // メインスレッド（ECS側）が、溜まった手紙を一気に回収するための関数
    std::vector<JoltCollisionEvent> GetAndClearEvents();

private:
    std::mutex                      m_mutex;  // スレッド競合を防ぐための「鍵」
    std::vector<JoltCollisionEvent> m_events; // 溜まった手紙を入れるポスト
};