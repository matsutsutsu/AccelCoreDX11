#include "PlayerHUDSystem.h"
#include "Engine/UI/Widgets/UIProgressBar.h"
#include "PlayerHUD.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void PlayerHUDSystem::Update(float dt)
{
    // 1. Resourceから掲示板を取得（ModelRenderSystemと同じアプローチ）
    if (!_world->HasResource<PlayerHUDData>())
    {
        _world->AddResource<PlayerHUDData>();
    }
    auto& hudData = _world->GetResource<PlayerHUDData>();

    // 対象となるエンティティ（プレイヤー）を走査
    // 2. プレイヤーのHPを計算して、掲示板に書き込むだけ！
    ForEach([&](const TPSPlayerComponent& player, const StaminaComponent& stamina) {
        float current = (std::max)(0.0f, stamina.current);
        hudData.staminaRatio = current / stamina.maxStamina;
        });
}
REGISTER_RENDER_SYSTEM(PlayerHUDSystem, Priority::RenderStage::R10_UI);