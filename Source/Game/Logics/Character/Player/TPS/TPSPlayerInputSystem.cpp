#include "TPSPlayerInputSystem.h"
#include "TPSPlayerComponent.h"

#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"

void TPSPlayerInputSystem::Update(float dt) 
{
    if (!_world->HasResource<std::shared_ptr<IInputAPI>>()) return;
    auto inputAPI = _world->GetResource<std::shared_ptr<IInputAPI>>();

    ForEach([&](TPSPlayerComponent& tps, AnimParametersComponent& animParams)
        {
            auto fbHash = CCL::Utils::HashString(tps.config.axis.moveFB);
            auto lrHash = CCL::Utils::HashString(tps.config.axis.moveLR);
            auto dodgeHash = CCL::Utils::HashString(tps.config.action.dodge);
            auto attackHash = CCL::Utils::HashString(tps.config.action.attack);
            auto decideHash = CCL::Utils::HashString(tps.config.action.decide); // 追加
            auto jumpHash = CCL::Utils::HashString(tps.config.action.jump);
            auto TargetingHash = CCL::Utils::HashString(tps.config.action.Targeting);
            auto lockOnHash = CCL::Utils::HashString(tps.config.action.lockOn);

            tps.input.moveInput.y = inputAPI->GetAxis(fbHash);
            tps.input.moveInput.x = inputAPI->GetAxis(lrHash);

            // 状態維持系（ホールド）
            tps.input.isSprintHeld = inputAPI->GetAction(CCL::Utils::HashString(tps.config.action.sprint));
            tps.input.isTargeting = inputAPI->GetAction(TargetingHash);
            tps.input.isGuardHeld = inputAPI->GetAction(CCL::Utils::HashString(tps.config.action.guard));
            tps.input.isLockOnTriggered = inputAPI->GetAction(lockOnHash);

            // 単発実行系（トリガー）
            tps.input.isDodgeTriggered = inputAPI->GetActionTriggered(dodgeHash);
            tps.input.isAttackTriggered = inputAPI->GetAction(attackHash);
            tps.input.isTargetDecided = inputAPI->GetActionTriggered(decideHash); // これをTargetingSystemで使用
            tps.input.isJumpTriggered = inputAPI->GetActionTriggered(jumpHash);

            animParams.SetBool(CCL::Utils::HashString(tps.config.action.sprint), tps.input.isSprintHeld);

        });
}

REGISTER_LOGIC_SYSTEM(TPSPlayerInputSystem, Priority::LogicStage::L01_Input);