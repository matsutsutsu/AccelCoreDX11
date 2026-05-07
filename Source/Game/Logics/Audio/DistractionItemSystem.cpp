#include "DistractionItemSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Logger.h"
#include "Game/Logics/AI/NavAI/Events/AISoundEvent.h"
#include "DistractionItemComponent.h" 
#include "Engine/Audio/AudioEvents.h" 
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// ベクトルの長さを計算するためのインクルード
#include <DirectXMath.h>

void DistractionItemSystem::Initialize() {
    ListenEvent<JoltCollisionEvent>([this](const JoltCollisionEvent& e) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_frameEvents.push_back(e);
        });
}


void DistractionItemSystem::CheckAndTriggerSound(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& relVel) {
    auto* item = _world->GetComponent<DistractionItemComponent>(entity);
    if (!item) return;
    if (item->hasTriggered) return;

    // =======================================================
    //  衝撃の強さ（相対速度のベクトル長）を計算
    // =======================================================
    DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&relVel);
    float impactSpeed = DirectX::XMVectorGetX(DirectX::XMVector3Length(v));

    //  閾値より小さければ「そっと置かれた」とみなして無視する
    if (impactSpeed < item->bounceVelocityThreshold) {
        return;
    }

    item->hasTriggered = true;

    // ① AIの脳に「音が鳴った事実」を伝達する
    AISoundEvent aiEv;
    aiEv.position = { pos.x, pos.y, pos.z };
    aiEv.volumeRadius = item->volumeRadius;
    _world->GetEventBus().Publish(aiEv);

    // ② プレイヤーの耳(FMOD)に「実際の音」を鳴らす
    if (item->fmodEventHash != 0) {
        PlaySoundEvent fmodEv;
        fmodEv.eventHash = item->fmodEventHash;
        fmodEv.position = { pos.x, pos.y, pos.z };
        _world->GetEventBus().Publish(fmodEv);
    }

    // ログに衝撃の速度を出力しておくとデバッグで便利です
    CCL_LOG_INFO(LogCategory::Core, "Distraction Item hit with speed %.1f m/s! Sound triggered.", impactSpeed);
}

void DistractionItemSystem::Update(float dt) {
    std::vector<JoltCollisionEvent> currentEvents;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentEvents = std::move(m_frameEvents);
        m_frameEvents.clear();
    }

    if (currentEvents.empty()) return;

    for (const auto& ev : currentEvents) {
        CheckAndTriggerSound(ev.entityA, ev.contactPosition, ev.relativeVelocity);
        CheckAndTriggerSound(ev.entityB, ev.contactPosition, ev.relativeVelocity);
    }
}

REGISTER_LOGIC_SYSTEM(DistractionItemSystem, Priority::LogicStage::L06_Resolution);