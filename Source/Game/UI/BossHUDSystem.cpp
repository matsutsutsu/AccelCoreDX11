#include "BossHUDSystem.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <algorithm>

void BossHUDSystem::Update(float dt)
{
    // リソース（掲示板）が存在しなければ作成
    if (!_world->HasResource<BossHUDData>()) {
        _world->AddResource<BossHUDData>();
    }
    auto& hudData = _world->GetResource<BossHUDData>();

    bool bossFound = false;

    // BossのBTコンポーネントとHealthを持つエンティティを探す
    ForEach([&](const HealthComponent& health, const BehaviorTreeComponent& btComp) {

        // 味方（プレイヤー）のHPを拾わないためのフェイルセーフ
        if (health.team != TeamID::Enemy) return;

        // 最新のHP割合を計算してリソースに書き込む
        float current = (std::max)(0.0f, health.currentHealth);
        if (health.maxHealth > 0.0f) {
            hudData.hpRatio = current / health.maxHealth;
        }

        hudData.currentPhase = btComp.blackboard.currentPhase;
        bossFound = true;
        });

    // ボスが存在すればUIを表示、いなければ非表示
    hudData.isVisible = bossFound;
}

// PlayerHUDSystemと同じUIフェーズで更新する
REGISTER_LOGIC_SYSTEM(BossHUDSystem, Priority::RenderStage::R10_UI);