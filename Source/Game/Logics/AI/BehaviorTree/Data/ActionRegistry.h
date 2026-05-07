/**
 * @file ActionRegistry.h
 * @brief AIの行動・条件のIDと、エディタ表示用の辞書（Single Source of Truth）
 */
#pragma once
#include <cstdint>
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeData.h"

 // ============================================================================
 // 【単一情報源】AIの「行動」と「条件」のID定義
 // 新しいアクションを追加するときは、必ずこのファイルにIDを追加する！
 // ============================================================================
namespace BossAI_ID {
    // --- 条件 (Conditions: 1〜99) ---
    inline constexpr ActionID Cond_RangeClose = 1;  // (旧:PhaseMove) 距離が近い
    inline constexpr ActionID Cond_RangeMedium = 2; // (旧:PhaseMelee) 距離が中程度
    inline constexpr ActionID Cond_IsPhase2 = 3;    // フェーズ2以降か

    // --- 行動 (Actions: 100〜) ---
    inline constexpr ActionID Act_MeleeAttack = 100;
    inline constexpr ActionID Act_ChargeAttack = 101;
    inline constexpr ActionID Act_Move = 102;
    inline constexpr ActionID Act_DroneOrbit = 103;
    inline constexpr ActionID Act_DroneSequential = 104;
    inline constexpr ActionID Act_Wait1s = 105;
    inline constexpr ActionID Act_Wait3s = 106;
    inline constexpr ActionID Act_DroneDeathRing = 107;
    inline constexpr ActionID Act_DroneAegisShield = 108;
}

// ============================================================================
// エディタUI表示用の辞書（上のIDと必ずセットで書く）
// ============================================================================
struct ActionEntry {
    ActionID id;
    const char* name;
    const char* category;
};

// 行動(Action)の辞書
inline constexpr ActionEntry g_ActionRegistry[] = {
    { 0,   "なし (None)", "None" },
    { BossAI_ID::Act_MeleeAttack, "近接攻撃 (Melee Attack)", "Attack" },
    { BossAI_ID::Act_ChargeAttack, "突進 (Charge Attack)",    "Attack" },
    { BossAI_ID::Act_Move, "移動 (Move To Target)",   "Move"   },
    { BossAI_ID::Act_DroneOrbit, "ドローン：円陣展開 (Drone Orbit)", "Drone" },
    { BossAI_ID::Act_DroneSequential, "ドローン：順番突撃 (Drone Sequential)", "Drone" },
    { BossAI_ID::Act_Wait1s, "待機 1秒 (Wait 1s)", "Common" },
    { BossAI_ID::Act_Wait3s, "待機 3秒 (Wait 3s)", "Common" },
    { BossAI_ID::Act_DroneDeathRing, "ドローン：処刑の輪 (Death Ring)", "Drone" },
    { BossAI_ID::Act_DroneAegisShield, "ドローン：絶対防衛 (Aegis Shield)", "Drone" },
};
inline constexpr int g_ActionRegistryCount = sizeof(g_ActionRegistry) / sizeof(ActionEntry);

// 条件(Condition)の辞書（★ロジックと一致するように修正済み）
inline constexpr ActionEntry g_ConditionRegistry[] = {
    { 0, "なし (None)", "None" },
    { BossAI_ID::Cond_RangeClose,  "ターゲットが近い (Distance < 5m)", "Range" },
    { BossAI_ID::Cond_RangeMedium, "ターゲットが中距離 (Distance 5-10m)", "Range" },
    { BossAI_ID::Cond_IsPhase2,    "フェーズ 2 (後半戦) (Phase >= 2)",  "State"  },
};
inline constexpr int g_ConditionRegistryCount = sizeof(g_ConditionRegistry) / sizeof(ActionEntry);