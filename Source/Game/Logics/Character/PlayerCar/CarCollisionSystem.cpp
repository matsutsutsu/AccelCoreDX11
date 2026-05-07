#include "CarCollisionSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include "Engine/GamePlay/Core/DestroyTag.h"

#include "PlayerCarSphereTag.h"
#include "Game/Logics/Character/Enemy/EnemyComponent.h" // パスは適宜合わせてください

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;

// ============================================================================
// 初期化と終了処理
// ============================================================================
void CarCollisionSystem::Initialize() {
    // 起動時に1回だけ、EventBusにコールバックを登録する
    ListenEvent<JoltCollisionEvent>([this](const auto& ev) {
        this->OnCollision(ev);
        });
}

CarCollisionSystem::~CarCollisionSystem() {}

// ============================================================================
// コールバック (別スレッドから呼ばれる可能性があるため最小限にする)
// ============================================================================
void CarCollisionSystem::OnCollision(const JoltCollisionEvent& ev) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(ev);
}

// ============================================================================
// メインループ (安全なメインスレッドで処理)
// ============================================================================
void CarCollisionSystem::Update(float dt) {

    // イベントを取り出してキューを空にする
    std::vector<JoltCollisionEvent> eventsToProcess;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_events.empty()) return;
        eventsToProcess = std::move(m_events);
        m_events.clear();
    }

    if (!_world->HasResource<JoltPhysicsManager>()) return;
    JPH::BodyInterface& bodyInterface = _world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    for (const auto& ev : eventsToProcess) {
        EntityID sphereID = InvalidEntityID;
        EntityID enemyID = InvalidEntityID;

        // O(1) の超高速なタグ判定で「車」と「敵」の衝突かを見極める
        if (_world->HasComponent<PlayerCarSphereTag>(ev.entityA) && _world->HasComponent<EnemyComponent>(ev.entityB)) {
            sphereID = ev.entityA;
            enemyID = ev.entityB;
        }
        else if (_world->HasComponent<PlayerCarSphereTag>(ev.entityB) && _world->HasComponent<EnemyComponent>(ev.entityA)) {
            sphereID = ev.entityB;
            enemyID = ev.entityA;
        }

        if (sphereID == InvalidEntityID || enemyID == InvalidEntityID) continue;
        if (_world->HasComponent<Tag::DestroyTag>(enemyID)) continue;

        // 車のスピードを取得
        auto* sphereHandle = _world->GetComponent<JoltHandleComponent>(sphereID);
        if (!sphereHandle) continue;

        JPH::Vec3 vel = bodyInterface.GetLinearVelocity(sphereHandle->bodyID);
        float speed = vel.Length();

        // ★ 時速(スピード)判定で敵を破壊！
        if (speed > 15.0f) {
            _world->AddComponent<Tag::DestroyTag>(enemyID);
        }
    }
}

REGISTER_LOGIC_SYSTEM(CarCollisionSystem, Priority::LogicStage::L06_Resolution);