/**
 * @file CombatAnimationSyncSystem.h
 * @brief アニメーションのシーケンスデータからイベントを読み取り、武器のHitbox状態を同期する
 */
#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Animation/AnimatorComponent.h"
#include "Game/Logic/Combat/CombatRosterComponent.h"

class CombatAnimationSyncSystem : public CCL::ECS::IfSystem<CombatAnimationSyncSystem,
    CCL::ECS::Read<AnimatorComponent>,
    CCL::ECS::Read<CombatRosterComponent>> {
public:
    CombatAnimationSyncSystem() : IfSystem("CombatAnimationSyncSystem") {}
    void Update(float dt) override;
};