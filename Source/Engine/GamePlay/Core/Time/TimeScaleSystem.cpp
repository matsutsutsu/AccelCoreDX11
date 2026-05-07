#include "TimeScaleSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h" // 優先度Enumへのパスは適宜調整してください
#include <algorithm>

TimeScaleSystem::TimeScaleSystem() : IfSystem("TimeScaleSystem")
{
}

void TimeScaleSystem::Initialize()
{
}

void TimeScaleSystem::Update(float rawDt)
{
    // 親時計のリソースが存在しなければ何もしない
    if (!_world || !_world->HasResource<TimeContext>()) return;

    // O(1)で超高速に親時計の情報を取得
    auto& ctx = _world->GetResource<TimeContext>();

    // =========================================================
    //  親時計（世界全体）のヒットスロー計算
    // =========================================================
    if (ctx.globalHitStopTimer > 0.0f) {
        ctx.globalHitStopTimer -= rawDt; // 現実時間で減算

        if (ctx.globalHitStopTimer <= 0.0f) {
            ctx.globalHitStopTimer = 0.0f;
            ctx.globalScale = ctx.baseGlobalScale; // 元の速度に戻る
        }
        else {
            // 全体を超スローにする
            ctx.globalScale = ctx.baseGlobalScale * ctx.globalFreezeScale;
        }
    }
    else {
        // ヒットスローが終わっていれば基本スケールを維持
        ctx.globalScale = ctx.baseGlobalScale;
    }

    // 【マルチスレッド超並列実行】
    // チャンク単位で分割され、全コアを使って腕時計の時間を一斉に合わせる
    ForEachParallel([&](TimeState& time) {

        // 1. 個別のヒットストップ処理
        if (time.hitStopTimer > 0.0f) {
            time.hitStopTimer -= rawDt; // 実際の時間で減算

            if (time.hitStopTimer <= 0.0f) {
                time.hitStopTimer = 0.0f; // 停止終了
            }
            else {
                // ヒットストップ中のdtを計算して即終了（スケール計算はスキップ）
                time.localDt = rawDt * time.freezeScale;
                return;
            }
        }

        // 2. 勢力ごとのスローモーション計算
        float scale = ctx.globalScale;
        switch (time.group) {
        case TimeGroup::Player:      scale *= ctx.playerScale; break;
        case TimeGroup::Enemy:       scale *= ctx.enemyScale;  break;
        case TimeGroup::Environment: /* 環境用スケールがあれば乗算 */ break;
        default: break;
        }

        // 3. 今フレームの最終的な実効dtを確定
        time.localDt = rawDt * scale;
        });
}


// ★ 最重要: 他のどのLogicシステムよりも先に時間を計算するため、PrePhysics等の最も早い段階に登録する
REGISTER_LOGIC_SYSTEM(TimeScaleSystem, Priority::LogicStage::L01_TimeScale)
