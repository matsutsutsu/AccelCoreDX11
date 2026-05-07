#include "TimeScaleSetupSystem.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;


TimeScaleSetupSystem::TimeScaleSetupSystem() : IfSystem("TimeScaleSetupSystem") {}

void TimeScaleSetupSystem::Update(float dt)
{
    // 追加対象のIDを一時保存するリスト（Foreach中の構造変更を避けるため）
    std::vector<EntityID> needsTimeState;

    // 1. Transformを持つが TimeState を持たないエンティティを抽出
    ForEachWithID([&](EntityID id, const TransformComponent& transform) {
        if (!_world->HasComponent<TimeState>(id)) {
            needsTimeState.push_back(id);
        }
        });

    // 2. 足りないエンティティに一括付与
    // ※これはプレハブ側の設定漏れに対する救済措置であり、
    // 実行中に頻発する場合はエディタ側でPrefabを修正すべきです。
    for (auto id : needsTimeState) {
        _world->AddComponent<TimeState>(id);

        // 警告ログを出すことで、プログラマにプレハブの修正を促す（DODの観点から重要）
        CCL_LOG_WARN(LogCategory::Game,
            "TimeState was automatically added to Entity %llu. "
            "To avoid chunk fragmentation, please add TimeState in the Editor Prefab.", id);
    }
}


// 初期化やセットアップを行うため、TimeScaleSystem本体よりも早い段階に登録する
REGISTER_LOGIC_SYSTEM(TimeScaleSetupSystem, Priority::LogicStage::L01_TimeSetup)