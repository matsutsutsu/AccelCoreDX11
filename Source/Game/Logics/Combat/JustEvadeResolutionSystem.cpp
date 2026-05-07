#include "JustEvadeResolutionSystem.h"

#include "Game/Logics/Combat/CombatComponents.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Platform/Logger.h"
#include "Game/Logics/Character/Player/PlayerStateComponent.h"
#include "Game/Logics/Combat/StaminaComponent.h"

// ヒットストップを付与するために時間をインクルード
#include "Engine/GamePlay/Core/Time/TimeState.h" 
#include <algorithm> // std::max 用
using namespace CCL::ECS;

void JustEvadeResolutionSystem::Update(float dt)
{
    ForEachWithID([this](EntityID eventEntity, JustEvadeEventComponent& evData)
    {


            // 1. 世界のグローバルタイマーをスローにする（TimeContextリソースへのアクセス）
            if (_world->HasResource<TimeContext>()) {
                auto& timeCtx = _world->GetResource<TimeContext>();
                timeCtx.globalHitStopTimer = evData.hitStopDuration;
                timeCtx.globalFreezeScale = evData.hitStopFreezeScale;
            }

            if (!_world->HasComponent<PlayerStateTag::CounterAttackOpportunity>(evData.targetID)) 
            {
                // タグの代わりに、時間制限付きの反撃権を付与
                _world->AddComponent<PlayerStateTag::CounterAttackOpportunity>(evData.targetID);
                // 既に持っている場合は、Patchを使って時間をリセット（リフレッシュ）
                _world->PatchComponent<PlayerStateTag::CounterAttackOpportunity>(evData.targetID, [](auto& comp) {
                    comp.remainingTime = 2.5f;
                    });
            }

            // 3. (オプション) スタミナを即時全回復させる等のボーナス処理
            if (auto* stam = _world->GetComponent<StaminaComponent>(evData.targetID))
            {
                stam->current = stam->maxStamina;
            }


            // 4. 使い終わったイベントエンティティを破棄
            _world->Destroy(eventEntity);
        });
}

// ダメージ解決と同じフェーズで実行
REGISTER_LOGIC_SYSTEM(JustEvadeResolutionSystem, Priority::LogicStage::L06_Resolution);