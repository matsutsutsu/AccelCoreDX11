#include "FPSPlayerInputSystem.h"
#include "FPSPlayerComponent.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include <memory>

using namespace CCL::ECS;

void FPSPlayerInputSystem::Update(float dt)
{
    if (!_world->HasResource<std::shared_ptr<IInputAPI>>()) return;
    auto inputAPI = _world->GetResource<std::shared_ptr<IInputAPI>>();

    ForEach([&](FPSPlayerComponent& player) {
        // コンポーネント内の config (std::string) を用いて API から値を取得
        player.input.moveForward = inputAPI->GetAxis(CCL::Utils::HashString(player.config.axis.moveFB));
        player.input.moveRight = inputAPI->GetAxis(CCL::Utils::HashString(player.config.axis.moveLR));
        player.input.isRunPressed = inputAPI->GetAction(CCL::Utils::HashString(player.config.action.dash));

        //player.input.isHidePressed = inputAPI->GetAction(CCL::Utils::HashString(player.config.action.hide));
        uint32_t hideHash = CCL::Utils::HashString(player.config.action.hide);

        // 押された瞬間の判定 (既存)
        player.input.isHidePressed = inputAPI->GetActionTriggered(hideHash);
        // 押し続けているかどうかの状態 (追加)
        player.input.isHideHolding = inputAPI->GetAction(hideHash);
        });
}

// システムをロジック更新の入力ステージ(L01)に自動登録
REGISTER_LOGIC_SYSTEM(FPSPlayerInputSystem, Priority::LogicStage::L01_Input);