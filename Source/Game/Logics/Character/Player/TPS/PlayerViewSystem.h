#pragma once
#include "ECS/System/CCL_System.h"

// 前方宣言
struct TPSPlayerComponent;
struct TPSPlayerStateComponent;
struct StaminaComponent;
struct ModifierStatusComponent;
struct PlayerViewComponent;

/**
 * プレイヤーの状態に基づいてカメラのFOV、ボビング、傾きなどの演出を制御するシステム
 */
class PlayerViewSystem
    : public CCL::ECS::IfSystem<PlayerViewSystem,
    CCL::ECS::Write<PlayerViewComponent>,
    CCL::ECS::Write<TPSPlayerComponent>,
    CCL::ECS::Read<TPSPlayerStateComponent>,
    CCL::ECS::Read<StaminaComponent>,
    CCL::ECS::Read<ModifierStatusComponent>>
{
public:
    PlayerViewSystem() : IfSystem("PlayerViewSystem") {}
    virtual ~PlayerViewSystem() = default;

    void Update(float dt) override;
    CCL::ECS::EntityID FindClosestEnemy(CCL::ECS::EntityID playerID,float TargetRange = 50, CCL::ECS::EntityID currentTarget = 0);
};