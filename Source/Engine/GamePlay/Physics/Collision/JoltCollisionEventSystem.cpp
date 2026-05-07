#include "JoltCollisionEventSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void JoltCollisionEventSystem::Update(float dt)
{
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    auto &joltManager = _world->GetResource<JoltPhysicsManager>();
    
    // 1. ポストの場所を取得
    JoltContactListener* listener = joltManager.GetContactListener();
    if (!listener) return;

    // 2. 溜まっている手紙（イベント）を一気に回収し、ポストを空にする
    // ※内部でミューテックス（鍵）がかかるためスレッドセーフ
    std::vector<JoltCollisionEvent> events = listener->GetAndClearEvents();

    // 3. 回収した手紙を、ECSの放送局（EventBus）に流す
    for (const auto& ev : events) {
        
        // ★注意: あなたのエンジンのEventBusのAPIに合わせて書き換えてください。
        // （例：_world->PublishEvent(ev); や _world->GetEventBus().Publish(ev); など）
        _world->GetEventBus().Publish(ev); 
        
    }
}

// ★絶対のルール：物理計算(Step)が終わった【後】に実行すること！
REGISTER_LOGIC_SYSTEM(JoltCollisionEventSystem, Priority::LogicStage::L05_Collision);