#include "StaminaSystem.h"

#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Game/Logic/System/Modifier/ModifierComponent.h"

#include <algorithm> // std::min / std::max

#include <memory>

using namespace CCL::ECS;

static constexpr uint32_t SID_FATIGUE = 0xFAB1;

void StaminaSystem::Update(float rawDt)
{
    ForEachWithID([&](EntityID entityID, StaminaComponent& stam,
        const ModifierStatusComponent& modStatus,
        const TimeState& time)
        {
            // ★ 各自の時計を使用
            float dt = time.localDt;

            // ModifierComponent は Add/Remove 操作のために取得
            auto* modComp = _world->GetComponent<ModifierComponent>(entityID);

            // --- 1. キャッシュされた Mod 適用済みの値を計算 ---
            // MaxStamina は「加算値」、Rate系は「倍率」として設計した Status を使用
            float finalMax = stam.maxStamina + modStatus.staminaMaxAdd;
            float finalConsume = stam.consumeRate * modStatus.staminaConsumeMult;
            float finalRecover = stam.recoveryRateBase * modStatus.staminaRecoverMult;
            float finalDelayTime = stam.recoveryDelayTime + modStatus.staminaRecoverDelayAdd;

            // --- A. 疲労の開始判定と Modifier 付与 ---
            // ※疲労フラグが立った瞬間に Mod リストに登録し、以降は Status 側で自動計算される
            if (stam.isFatigued && modComp && !PlayerMod::HasModifier(modComp, SID_FATIGUE))
            {
                // ここでの AddModifier は Mod リストを更新し Dirty フラグを立てる
                PlayerMod::AddModifier(modComp, "Fatigue: Slowdown", PlayerMod::ParamType::Move_OverallSpeed,
                    SID_FATIGUE, stam.fatigueMoveSpeedMult, -1.0f, true);

                PlayerMod::AddModifier(modComp, "Fatigue: Exhausted", PlayerMod::ParamType::Stam_RecoverRate,
                    SID_FATIGUE, stam.fatigueRecoveryMultiplier, -1.0f, true);

                PlayerMod::AddModifier(modComp, "Fatigue: Tunnel Vision", PlayerMod::ParamType::View_BaseFOV,
                    SID_FATIGUE, stam.fatigueFOVMult, -1.0f, true);

                // --- 停止(Idle)用の演出倍率 ---
                PlayerMod::AddModifier(modComp, "Fatigue: Idle Shake", PlayerMod::ParamType::View_IdleBobAmount,
                    SID_FATIGUE, stam.fatigueIdleBobAmountMult, -1.0f, true);
                PlayerMod::AddModifier(modComp, "Fatigue: Idle Speed", PlayerMod::ParamType::View_IdleBobSpeed,
                    SID_FATIGUE, stam.fatigueIdleBobSpeedMult, -1.0f, true);

                // --- 歩行(Walk)用の演出倍率 ---
                PlayerMod::AddModifier(modComp, "Fatigue: Walk Shake", PlayerMod::ParamType::View_WalkBobAmount,
                    SID_FATIGUE, stam.fatigueWalkBobAmountMult, -1.0f, true);
                PlayerMod::AddModifier(modComp, "Fatigue: Walk Speed", PlayerMod::ParamType::View_WalkBobSpeed,
                    SID_FATIGUE, stam.fatigueWalkBobSpeedMult, -1.0f, true);

                // --- 共通の眩暈（Tilt） ---
                // ここは先ほどの設計通り、どの状態でも「フラフラ」を出すための専用パラメータへ
                // 停止中と歩行中で Tilt の強度を使い分けたい場合は、システム側で status.viewFatigueTiltMult を参照する際に
                // 状態を見て stam.fatigueIdleTiltMult か WalkTiltMult かを切り替えます。
                PlayerMod::AddModifier(modComp, "Fatigue: Dizziness", PlayerMod::ParamType::View_FatigueTiltMult,
                    SID_FATIGUE, 1.0f, -1.0f, false);
            }

            // --- B. 疲労の解除判定 ---
            // 閾値も Status で補正可能に設計済み（staminaFatigueThresholdAdd）
            float currentThreshold = stam.fatigueRecoveryThreshold + (modStatus.staminaFatigueThresholdAdd / finalMax);

            if (stam.isFatigued && stam.current >= (finalMax * currentThreshold)) {
                stam.isFatigued = false;
                if (modComp) {
                    PlayerMod::RemoveModifierBySourceID(modComp, SID_FATIGUE);
                }
            }

            // 1. 固定値消費（回避など）の即時処理
            if (stam.instantConsumeRequest > 0.0f) {
                if (!stam.isFatigued) {
                    // 現在値からリクエスト分を差し引く
                    stam.current -= stam.instantConsumeRequest;

                    // 消費が発生したので回復ディレイをセット
                    stam.recoveryDelayTimer = finalDelayTime;
                }

                // 消費の成否に関わらず、リクエストは毎フレームクリアする
                stam.instantConsumeRequest = 0.0f;

                // 0を下回った場合の疲労判定
                if (stam.current <= 0.0f) {
                    stam.current = 0.0f;
                    stam.isFatigued = true;
                }
            }


            // --- C. スタミナ増減処理 ---
            if (stam.isConsuming) {
                stam.current -= finalConsume * dt;
                stam.recoveryDelayTimer = finalDelayTime;

                if (stam.current <= 0.0f) {
                    stam.current = 0.0f;
                    stam.isFatigued = true;
                }
            }
            else {
                if (stam.recoveryDelayTimer > 0.0f) {
                    stam.recoveryDelayTimer -= dt;
                }

                if (stam.recoveryDelayTimer <= 0.0f && stam.current < finalMax) {
                    // 移動中の回復減衰 factor
                    float moveFactor = (stam.isMoving) ? stam.moveRecoveryMultiplier : 1.0f;

                    // すでに finalRecover には疲労等のデバフ（modStatus.staminaRecoverMult）が反映されている
                    stam.current = (std::min)(stam.current + (finalRecover * moveFactor) * dt, finalMax);
                }
            }

            stam.isConsuming = false;
        });
}

// コンポーネントの自動登録
REGISTER_LOGIC_SYSTEM(StaminaSystem, Priority::LogicStage::L02_Update)