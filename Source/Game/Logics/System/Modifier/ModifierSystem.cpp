#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "ModifierSystem.h"
#include "ModifierComponent.h"

void PlayerModifierSystem::Update(float dt)
{
    ForEach([&](ModifierComponent& mod, ModifierStatusComponent& status)
        {
            bool listChanged = false;

            // 1. 寿命更新
            for (auto& instance : mod.activeMods) {
                if (!instance.IsPermanent()) {
                    instance.duration -= dt;
                    if (instance.duration <= 0.0f) listChanged = true;
                }
            }

            // 2. 期限切れの削除
            if (listChanged) {
                mod.activeMods.erase(
                    std::remove_if(mod.activeMods.begin(), mod.activeMods.end(),
                        [](const auto& m) { return !m.IsPermanent() && m.duration <= 0.0f; }),
                    mod.activeMods.end()
                );
                mod.isDirty = true;
            }

            // 3. パラメータの再集計
            if (mod.isDirty) {
                // デフォルト値へのリセット
                status.moveWalkSpeedMult = 1.0f;
                status.moveRunSpeedMult = 1.0f;
                status.moveAccelMult = 1.0f;

                status.staminaMaxAdd = 0.0f;
                status.staminaConsumeMult = 1.0f;
                status.staminaRecoverMult = 1.0f;
                status.staminaRecoverDelayAdd = 0.0f;
                status.staminaFatigueThresholdAdd = 0.0f;

                status.viewBaseFOVAdd = 0.0f;
                // 揺れ（Bobbing）のリセット
                status.viewIdleBobAmountMult = 1.0f;
                status.viewIdleBobSpeedMult = 1.0f;
                status.viewWalkBobAmountMult = 1.0f;
                status.viewWalkBobSpeedMult = 1.0f;
                status.viewRunBobAmountMult = 1.0f;
                status.viewRunBobSpeedMult = 1.0f;
                // 傾き（Tilt）のリセット
                status.viewRunTiltAmountMult = 1.0f;
                status.viewFatigueTiltMult = 0.0f; // 0.0ベース（加算的な倍率枠

                float overallMoveMult = 1.0f;

                for (const auto& m : mod.activeMods) {
                    // 特殊：全体補正
                    if (m.target == PlayerMod::ParamType::Move_OverallSpeed) {
                        overallMoveMult *= m.value;
                        continue;
                    }

                    // 各パラメータの振り分け
                    switch (m.target) {
                        // 移動
                    case PlayerMod::ParamType::Move_WalkSpeed:    status.moveWalkSpeedMult *= m.value; break;
                    case PlayerMod::ParamType::Move_RunSpeed:     status.moveRunSpeedMult *= m.value; break;
                    case PlayerMod::ParamType::Move_Acceleration: status.moveAccelMult *= m.value; break;

                        // スタミナ
                    case PlayerMod::ParamType::Stam_MaxStamina:   status.staminaMaxAdd += m.value; break;
                    case PlayerMod::ParamType::Stam_ConsumeRate:  status.staminaConsumeMult *= m.value; break;
                    case PlayerMod::ParamType::Stam_RecoverRate:  status.staminaRecoverMult *= m.value; break;
                    case PlayerMod::ParamType::Stam_RecoverDelay: status.staminaRecoverDelayAdd += m.value; break;
                    case PlayerMod::ParamType::Stam_FatigueThreshold: status.staminaFatigueThresholdAdd += m.value; break;

                    // --- ビュー：共通 ---
                    case PlayerMod::ParamType::View_BaseFOV:          status.viewBaseFOVAdd += m.value; break;

                        // --- ビュー：待機(Idle) ---
                    case PlayerMod::ParamType::View_IdleBobAmount:    status.viewIdleBobAmountMult *= m.value; break;
                    case PlayerMod::ParamType::View_IdleBobSpeed:     status.viewIdleBobSpeedMult *= m.value; break;

                        // --- ビュー：歩行(Walk) ---
                    case PlayerMod::ParamType::View_WalkBobAmount:    status.viewWalkBobAmountMult *= m.value; break;
                    case PlayerMod::ParamType::View_WalkBobSpeed:     status.viewWalkBobSpeedMult *= m.value; break;

                        // --- ビュー：走行(Run) ---
                    case PlayerMod::ParamType::View_RunBobAmount:     status.viewRunBobAmountMult *= m.value; break;
                    case PlayerMod::ParamType::View_RunBobSpeed:      status.viewRunBobSpeedMult *= m.value; break;
                    case PlayerMod::ParamType::View_RunTiltAmount:    status.viewRunTiltAmountMult *= m.value; break;

                        // --- ビュー：演出用(Fatigue等による強制Tilt) ---
                    case PlayerMod::ParamType::View_FatigueTiltMult:  status.viewFatigueTiltMult += m.value; break;
                    }
                }

                // 全体倍率を適用
                status.moveWalkSpeedMult *= overallMoveMult;
                status.moveRunSpeedMult *= overallMoveMult;
                status.moveAccelMult *= overallMoveMult;

                mod.isDirty = false;
            }
        });
}

REGISTER_LOGIC_SYSTEM(PlayerModifierSystem, Priority::LogicStage::L01_Input); // 状態更新の直後に実行