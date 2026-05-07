#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/Combat/HealthComponent.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeComponents.h"

#include "BossHUDData.h"

// ボスのHPを読み取って、UI表示用のデータに変換するシステム
class BossHUDSystem : public CCL::ECS::IfSystem<BossHUDSystem,
    CCL::ECS::Read<HealthComponent>,
    CCL::ECS::Read<BehaviorTreeComponent>>
{
public:
    BossHUDSystem() : IfSystem("BossHUDSystem") {}
    void Update(float dt) override;
};