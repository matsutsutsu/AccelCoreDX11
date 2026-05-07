#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/Combat/CombatComponents.h"

/**
 * @brief ジャスト回避イベントを解決し、ゲーム世界に結果を反映するシステム
 * 実行順序はダメージ解決と同等の L06_Resolution ステージ
 */
class JustEvadeResolutionSystem : public CCL::ECS::IfSystem<JustEvadeResolutionSystem,
    CCL::ECS::Write<JustEvadeEventComponent>>
{
public:
    JustEvadeResolutionSystem() : IfSystem("JustEvadeResolutionSystem") {}

    void Initialize() override {}
    void Update(float dt) override;
};