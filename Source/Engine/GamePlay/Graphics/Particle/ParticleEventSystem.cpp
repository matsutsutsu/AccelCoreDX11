#include "ParticleEventSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/GamePlay/Graphics/Particle/GPUParticleComponent.h"
#include "Engine/Serialization/Factory/Prefab.h"

#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Transform/PendingParentComponent.h"

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void ParticleEventSystem::Initialize()
{
    // 放送局（EventBus）に「エフェクトの手紙が来たら教えて！」と登録する
    // クラスのメンバ関数を呼ぶため、ラムダ式 [this] でキャプチャして登録します
    _world->GetEventBus().Subscribe<AnimEventEffectMessage>(
        [this](const AnimEventEffectMessage& msg) {
            this->OnEffectEvent(msg);
        }
    );
}


// 完全にイベント駆動で動くため、毎フレームの無駄な走査（Update）は不要です。負荷ゼロ！
void ParticleEventSystem::Update(float dt) {}


void ParticleEventSystem::OnEffectEvent(const AnimEventEffectMessage& msg)
{
    // 1. パスが空なら何もしない
    if (msg.effectPrefabPath.empty()) return;

    // 2. 誰がアニメーションしているか（発生源）の位置と向きを取得する
    auto* transform = _world->GetComponent<TransformComponent>(msg.entity);
    if (!transform) return;

    // 1. エフェクトPrefabを実体化させる（ルートのIDが返ってくる）
    CCL::ECS::EntityID spawnedRoot = Prefab::SpawnPrefab(*_world,
        msg.effectPrefabPath,
        transform->position,
        transform->rotation);

    // 2. ★追加：召喚したエフェクトをキャラクター(msg.entity)の子供にする
    if (spawnedRoot != CCL::ECS::InvalidEntityID && spawnedRoot != 0) {
        PendingParentComponent pending;
        pending.parentID = msg.entity; // アニメーションを再生した主を親に設定

        if (!_world->HasComponent<PendingParentComponent>(spawnedRoot)) {
            _world->AddComponent<PendingParentComponent>(spawnedRoot, pending);
        }
        else {
            _world->GetComponent<PendingParentComponent>(spawnedRoot)->parentID = msg.entity;
        }
    }

    //printf("[Effect] 召喚成功: %s (Entity: %llu の位置)\n", msg.effectPrefabPath.c_str(), msg.entity);
}


// システムの自動登録
REGISTER_LOGIC_SYSTEM(ParticleEventSystem, Priority::LogicStage::L02_Update);