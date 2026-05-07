#pragma once
#include <cstdint>
/**
 * @file TestEnemyActionComponent.h
 * @brief AIコマンドを受け取って動く敵のステータス定義（テスト用）
 */

 // ★ 追加：敵が現在実行中のアクション状態
enum class EnemyActionState : uint8_t {
    None,   // 待機またはウロウロ中
    Melee,  // 近接攻撃中
    Charge  // 突進中
};

struct TestEnemyActionComponent {
    float walkSpeed = 5.0f;  // 通常移動速度
    float chargeSpeed = 15.0f; // 突進（Charge）時の速度
    float turnSpeed = 10.0f; // 振り向きの滑らかさ

    // ★ 修正：bool ではなく、具体的な状態を記憶させる
    EnemyActionState currentState = EnemyActionState::None;

    float attackTimer = 0.0f;  // 攻撃中の硬直タイマー
    float moveTimer = 0.0f;    // ウロウロ用タイマー
};