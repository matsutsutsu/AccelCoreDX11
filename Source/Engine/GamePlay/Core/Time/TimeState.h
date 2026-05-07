#pragma once
#include <cstdint>

// ---------------------------------------------------------
// [時間グループ定義]
// 各エンティティがどの時間軸に属しているかを示すフラグ
// ---------------------------------------------------------
enum class TimeGroup : uint8_t {
    None = 0,
    Player = 1,
    Enemy = 2,
    Environment = 3,
    UI = 4
};

// ---------------------------------------------------------
// [世界の親時計 (World Resource)]
// O(1)でアクセス可能なグローバルな時間スケール設定
// ---------------------------------------------------------
struct TimeContext {
    // 元の世界の速度 (通常1.0)
    float baseGlobalScale = 1.0f;

    // 実際に各システムが読むスケール
    float globalScale = 1.0f;

    // --- 全体ヒットストップ（ヒットスロー）用 ---
    float globalHitStopTimer = 0.0f;
    float globalFreezeScale = 1.0f;

    float playerScale = 1.0f;  // プレイヤー勢力専用
    float enemyScale = 1.0f;   // 敵勢力専用
    float cameraScale = 1.0f;  // カメラ専用
};

// ---------------------------------------------------------
// [各自の腕時計 (Component)]
// ★重要: 動く全てのエンティティのArchetypeに最初から積んでおく
// ---------------------------------------------------------
struct TimeState {
    // 各システム（移動、アニメ等）がReadする「計算済みの実効デルタタイム」
    float localDt = 0.016f;

    // --- 以下、TimeScaleSystem専用の内部状態 ---
    float hitStopTimer = 0.0f;  // 0.0fなら平常。リアル時間(rawDt)で減算される
    float freezeScale = 0.0f;  // ヒットストップ中の進み具合（0=完全停止）
    TimeGroup group = TimeGroup::None;
};