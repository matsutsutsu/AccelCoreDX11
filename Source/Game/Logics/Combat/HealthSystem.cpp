#include "HealthSystem.h"
#include "Game/Logics/Combat/DeadTag.h"
#include "Game/Logics/GameEvent/GameEvents.h"


#include "Game/Logics/Character/Player/PlayerComponent.h"
#include <algorithm>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void HealthSystem::Update(float dt)
{

    // =======================================================================
    // フェーズ2：毎日の健康診断（全エンティティの定期更新）
    // =======================================================================
    ForEachWithID([&](CCL::ECS::EntityID id, HealthComponent &health) {
        // 1. 無敵時間のカウントダウン
        if (health.invincibilityTimer > 0.0f) {
            health.invincibilityTimer -= dt;
        }

        // 2. 死亡判定と Tag の付与
        if (health.currentHealth <= 0.0f) {
            health.currentHealth = 0.0f;

            // エンティティにまだ DeadTag が付いていなければ付与する
            if (!_world->HasComponent<Tag::DeadTag>(id)) {
                _world->AddComponent<Tag::DeadTag>(id);
            }
        }
    });

}


REGISTER_LOGIC_SYSTEM(HealthSystem, Priority::LogicStage::L06_HealthManagement);