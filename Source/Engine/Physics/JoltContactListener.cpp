#include "JoltContactListener.h"
#include <Jolt/Physics/Body/Body.h> // Bodyの中身(完全な型)を知るために絶対に必要

void JoltContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                         const JPH::ContactManifold& inManifold,
                                         JPH::ContactSettings& ioSettings)
{
    // 1. Joltの剛体に埋め込んでおいた ECSの EntityID を取り出す
    auto entityA = static_cast<CCL::ECS::EntityID>(inBody1.GetUserData());
    auto entityB = static_cast<CCL::ECS::EntityID>(inBody2.GetUserData());

    // 両方とも正しくEntityIDが取れなかった場合は無視（壁同士など）
    if (entityA == 0 && entityB == 0) return;

    // 2. 衝突座標と法線を計算
    // ★修正: GetBaseOffset() という関数ではなく、mBaseOffset という変数に直接アクセスする
    JPH::RVec3 pos = inManifold.mBaseOffset + inManifold.mRelativeContactPointsOn1[0];
    JPH::Vec3  normal = inManifold.mWorldSpaceNormal;
    JPH::Vec3 relVel = inBody1.GetLinearVelocity() - inBody2.GetLinearVelocity();

    JoltCollisionEvent newEvent;
    newEvent.entityA         = entityA;
    newEvent.entityB         = entityB;
    newEvent.contactPosition = {pos.GetX(), pos.GetY(), pos.GetZ()};
    newEvent.contactNormal   = {normal.GetX(), normal.GetY(), normal.GetZ()};
    // 計算した相対速度をイベントに乗せる
    newEvent.relativeVelocity = { relVel.GetX(), relVel.GetY(), relVel.GetZ() };

    // =======================================================
    // 3. スレッドセーフに配列へ追加（Traffic Control）
    // =======================================================
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(newEvent);
}

std::vector<JoltCollisionEvent> JoltContactListener::GetAndClearEvents()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<JoltCollisionEvent> copy = m_events;
    m_events.clear(); 
    
    return copy;
}