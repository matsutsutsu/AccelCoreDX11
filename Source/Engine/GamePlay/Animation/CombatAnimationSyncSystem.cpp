/**
 * @file CombatAnimationSyncSystem.cpp
 */
#include "CombatAnimationSyncSystem.h"
#include "Game/Logics/Combat/CombatComponents.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Platform/Logger.h"

using namespace CCL::ECS;

void CombatAnimationSyncSystem::Update(float dt) {

    // ボスやプレイヤーなど、Animatorと名簿(Roster)を持つルートエンティティを走査
    ForEachWithID([this](EntityID rootID, const AnimatorComponent& animator, const CombatRosterComponent& roster) {

        const AnimSequence* seq = animator.currentSequence;
        if (!seq) return;

        // =========================================================
        // 各武器（名簿に登録されている部位）ごとに状態を評価する
        // =========================================================
        for (int i = 0; i < roster.count; ++i) {
            const char* targetTag = roster.entries[i].tag; // "RightHand" など
            EntityID weaponID = roster.entries[i].id;

            auto* hitbox = _world->GetComponent<HitboxComponent>(weaponID);
            if (!hitbox) continue;

            // この部位に対して、現在の時間でアクティブなHitBoxイベントがあるか調べる
            bool shouldBeActive = false;

            for (const auto& ev : seq->events) {
                if (ev.eventName == "HitBox" && ev.stringParam == targetTag) {
                    // 現在の再生時間がイベントの区間内にあるか判定
                    if (animator.currentTimer >= ev.startTime && animator.currentTimer <= ev.endTime) {
                        shouldBeActive = true;
                        break;
                    }
                }
            }

            // =========================================================
            // エッジ検出とステートの転写
            // =========================================================
            if (shouldBeActive && !hitbox->isActive) {
                // 【立ち上がりエッジ】前回までOFFで、今フレームからONになった瞬間
                hitbox->hitCount = 0; // 多段ヒット防止の履歴をクリア
                memset(hitbox->hitTargets, 0, sizeof(hitbox->hitTargets)); // 履歴配列をゼロクリア
                hitbox->isActive = true;

                CCL_LOG_INFO(LogCategory::Game, "Hitbox Activated on '%s' (Entity %llu)", targetTag, weaponID);
            }
            else if (!shouldBeActive && hitbox->isActive) {
                // 【立ち下がりエッジ】区間を抜けた（またはアニメがキャンセルされた）瞬間
                hitbox->isActive = false;

                CCL_LOG_INFO(LogCategory::Game, "Hitbox Deactivated on '%s' (Entity %llu)", targetTag, weaponID);
            }
        }
        });
}

// L02_Update (AnimationSystem等) の後、L05_Collision の前に実行されるように登録する
REGISTER_LOGIC_SYSTEM(CombatAnimationSyncSystem, Priority::LogicStage::L02_PostUpdate);