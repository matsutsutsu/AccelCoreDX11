#pragma once

#include <DirectXMath.h>
#include "ECS/Common/CCL_Common.h"

// まず列挙型とデータ構造の最小単位を定義
namespace PlayerMod
{
    enum class ParamType {
        // --- 移動速度設定 ---
        Move_WalkSpeed,
        Move_RunSpeed,
        Move_Acceleration,
        Move_OverallSpeed, // 総合速度倍率 (Speed/Acc 全体に影響)

        // --- Stamina (スタミナ関連) ---
        Stam_MaxStamina,        // 最大値そのものを増やす
        Stam_ConsumeRate,       // 消費の速さ
        Stam_RecoverRate,       // 基本回復量
        Stam_RecoverDelay,      // 回復開始までの待機時間
        Stam_FatigueThreshold,  // 疲労解除のしきい値

        // --- ビュー・カメラ演出設定 (ViewComponent用) ---
        View_BaseFOV,           // 基本FOVへの補正
        View_IdleBobAmount,   // 待機時の揺れ幅
        View_IdleBobSpeed,    // 待機時の揺れ速度

        View_WalkBobAmount,   // 歩行時の揺れ幅
        View_WalkBobSpeed,    // 歩行時の揺れ速度

        View_RunBobAmount,    // 走行時の揺れ幅
        View_RunBobSpeed,     // 走行時の揺れ速度
        View_RunTiltAmount,   // 走行時の傾き

        View_FatigueTiltMult  // 追加で付与される「フラフラ」の倍率
    };

    struct ModifierInstance {
        char name[32];      // 表示用の名前（固定長にすることでPOD構造を維持）
        ParamType target;
        uint32_t sourceID;
        float value;
        float duration;
        bool isPercent;

        bool IsPermanent() const { return duration < 0.0f; }
    };
}
// これらを使うコンポーネントを定義
/**
 * @brief 計算済みの最終補正値を保持するコンポーネント
 * MovementSystem, StaminaSystem, ViewSystemなどはこちらをReadのみで参照する
 */
struct ModifierStatusComponent
{
    // 移動関連 (倍率)
    float moveWalkSpeedMult = 1.0f;
    float moveRunSpeedMult = 1.0f;
    float moveAccelMult = 1.0f;

    // スタミナ関連
    float staminaMaxAdd = 0.0f;       // 加算値
    float staminaConsumeMult = 1.0f;  // 倍率
    float staminaRecoverMult = 1.0f;  // 倍率
    float staminaRecoverDelayAdd = 0.0f; // 加算値 (秒)
    float staminaFatigueThresholdAdd = 0.0f;

    // ビュー関連
    float viewBaseFOVAdd = 0.0f;
    float viewIdleBobAmountMult = 1.0f;
    float viewIdleBobSpeedMult = 1.0f;

    float viewWalkBobAmountMult = 1.0f;
    float viewWalkBobSpeedMult = 1.0f;

    float viewRunBobAmountMult = 1.0f;
    float viewRunBobSpeedMult = 1.0f;

    float viewRunTiltAmountMult = 1.0f;
    float viewFatigueTiltMult = 0.0f; // 0.0なら無効。1.0なら「走行時並みのフラフラ」を強制
};

struct ModifierComponent
{
    std::vector<PlayerMod::ModifierInstance> activeMods;
    bool isDirty = true; // 内容が変わった時に計算を促すフラグ
};

// コンポーネントの中身にアクセスするユーティリティ関数を定義
namespace PlayerMod
{
    /**
     * @brief 有効なモディファイアをすべて適用した最終値を計算する
     */
    inline float CalculateFinalValue(float baseValue, ParamType type, const ModifierComponent* modComp) // ポインタに変更
    {
        // コンポーネントがない場合は補正なし
        if (!modComp) return baseValue;

        float flatAdd = 0.0f;
        float multiplier = 1.0f;

        for (const auto& mod : modComp->activeMods)
        {
            bool isDirectTarget = (mod.target == type);
            bool isOverallMoveTarget = (mod.target == ParamType::Move_OverallSpeed &&
                (type == ParamType::Move_WalkSpeed ||
                    type == ParamType::Move_RunSpeed ||
                    type == ParamType::Move_Acceleration));
            if (isDirectTarget || isOverallMoveTarget)
            {
                if (mod.isPercent)
                {
                    // mod.value が 1.2 なら 1.2倍される
                    // もし 20 (%) がそのまま入ってきていると 20倍 になるので注意
                    multiplier *= mod.value;
                }
                else
                {
                    flatAdd += mod.value;
                }
            }
        }

        return (baseValue + flatAdd) * multiplier;
    }

    inline void AddModifier(ModifierComponent* comp, const char* name, ParamType type, uint32_t sourceID, float val, float time, bool percent = false)
    {
        ModifierInstance inst{};
        strncpy_s(inst.name, name, sizeof(inst.name) - 1); // 名前をコピー
        inst.target = type;
        inst.sourceID = sourceID;
        inst.value = val;
        inst.duration = time;
        inst.isPercent = percent;

        comp->activeMods.push_back(inst);
        comp->isDirty = true; //再計算を予約する
    }

    inline void AddPermanentModifier(ModifierComponent* comp, const char* name, ParamType type, uint32_t sourceID, float val, bool percent = false)
    {
        ModifierInstance inst{};
        strncpy_s(inst.name, name, sizeof(inst.name) - 1); // 名前をコピー
        inst.target = type;
        inst.sourceID = sourceID;
        inst.value = val;
        inst.duration = -1.0f;
        inst.isPercent = percent;

        comp->activeMods.push_back(inst);
        comp->isDirty = true; //再計算を予約する
    }

    // 特定のSourceIDを持つモディファイアが存在するかチェック
    inline bool HasModifier(const ModifierComponent* comp, uint32_t sourceID)
    {
        for (const auto& mod : comp->activeMods)
        {
            if (mod.sourceID == sourceID) return true;
        }
        return false;
    }

    inline void RemoveModifierBySourceID(ModifierComponent* comp, uint32_t id)
    {
        comp->isDirty = true; //再計算を予約する

        comp->activeMods.erase(
            std::remove_if(comp->activeMods.begin(), comp->activeMods.end(),
                [id](const auto& m) { return m.sourceID == id; }),
            comp->activeMods.end()
        );
    }
}