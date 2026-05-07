/**
 * @file DamageResolutionSystem.cpp
 */
#include "DamageResolutionSystem.h"
#include "Game/Logics/Combat/CombatComponents.h"
#include "Game/Logics/Combat/HealthComponent.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Platform/Logger.h"

using namespace CCL::ECS;

void DamageResolutionSystem::Update(float dt) {

    // 一時的に生成されたダメージイベントを全て走査
    ForEachWithID([this](EntityID eventEntity, const DamageEventComponent& event) {

        // ターゲットのHealthを取得 (ランダムアクセスだが、L1/L2キャッシュの恩恵は受けやすい)
        if (auto* health = _world->GetComponent<HealthComponent>(event.targetID)) {

            // 無敵時間中ならダメージを無効化（処理はスキップされるがイベントは破棄される）
            if (health->invincibilityTimer <= 0.0f) {

                // HPの減算
                health->currentHealth -= event.finalDamage;

                // ダメージを受けた瞬間に無敵時間をリセット（多段ヒット・ハメ防止）
                health->invincibilityTimer = health->invincibilityDuration;

                CCL_LOG_INFO(LogCategory::Game, "Entity %llu took %f damage. Remaining HP: %f",
                    event.targetID, event.finalDamage, health->currentHealth);

                // 【拡張ポイント】
                // ここでヒットエフェクトやカメラシェイクのRequest(一時エンティティ)を
                // PendingOps経由でSpawnさせると、完全に疎結合なVFXシステムが構築できる。
            }
        }

        // ★最重要: 処理が終わった「イベントエンティティ」は必ず破棄する
        _world->Destroy(eventEntity);
        });
}



// L05の判定システムの直後、かつHealthSystem(死亡判定など)の前に実行する
REGISTER_LOGIC_SYSTEM(DamageResolutionSystem, Priority::LogicStage::L06_DamageApply);