/**
 * @file BossActionComponent.h
 * @brief AIコマンド(脳)を受け取って動く、ボス本体の物理・アニメーション状態（筋肉）
 */
#pragma once
#include <cstdint>
#include <SimpleMath.h>

 // アクションのタイミング定数を一括管理
namespace BossTimings {
    // --- 近接攻撃 ---
    constexpr float Melee_Duration = 1.0f;

    // --- チャージ攻撃 (合計 2.0秒 に延長) ---
    constexpr float Charge_Total = 2.0f; // 浮遊タメ(0.5s) + 突進(1.0s) + ブレーキ(0.5s)
    constexpr float Charge_DashStart = 1.5f; // 空中突進を開始する時間
    constexpr float Charge_BrakeTime = 0.5f; // ブレーキを開始する時間

    // --- ジャンプ攻撃 (合計 3.0秒 に延長) ---
    constexpr float JumpAttack_Total = 3.0f; // 地上タメ(0.5s) + 飛翔(1.5s) + 着地硬直(1.0s)
    constexpr float JumpAttack_Liftoff = 2.5f; // 実際に足が地面から離れる時間
    constexpr float JumpAttack_Land = 1.0f; // 地面に着地する時間

    // --- 回避行動 (合計 1.2秒 に延長して細分化) ---
    constexpr float Evade_Total = 1.8f; // 全体時間
    constexpr float Evade_Airborne = 1.6f; // 0.2秒の Start(タメ) の後、飛び退き開始
    constexpr float Evade_Land = 0.1f; // 残り0.2秒で End(着地) アニメーションへ

    // --- アニメーショントリガー ---
    constexpr float Melee_AnimTrigger = Melee_Duration;
    constexpr float Charge_AnimTrigger = Charge_Total;       // 開始と同時に浮遊＆チャージアニメ
    constexpr float JumpAttack_StartTrigger = JumpAttack_Total;   // 開始と同時にしゃがみ(タメ)アニメ

    // 怯みモーションの定数
    constexpr float Flinch_Duration = 0.4f; // 怯みで硬直する時間（アニメーションに合わせて調整）
}

enum class BossActionState : uint8_t {
    None,   // 待機
    Move,   // プレイヤーへの移動中
    Melee,  // 近接攻撃中
    Charge, // 突進中
    JumpAttack, // ジャンプ攻撃中
	Evade,  // 回避行動中
    Flinch, // 被弾して怯んでいる状態
};

struct BossActionComponent {
    // --- 12バイト型 (Vector3) ---
    DirectX::SimpleMath::Vector3 moveDirection = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 actionDirection = DirectX::SimpleMath::Vector3::Zero;

    // --- 4バイト型 (float: 性能パラメータ) ---
    float walkSpeed = 3.0f;
    float chargeSpeed = 25.0f;
    float turnSpeed = 10.0f;
    float evadeSpeed = 25.0f;
    float acceleration = 10.0f;
    float deceleration = 15.0f;
    float jumpAttackApexHeight = 8.0f;
    float chargeHoverVelocity = 6.0f;

    // --- 4バイト型 (float: ランタイム状態) ---
    float currentMoveSpeed = 0.0f;
    float dynamicJumpSpeed = 0.0f;
    float actionTimer = 0.0f;

    // --- 1バイト型 (enum / bool) ---
    BossActionState currentState = BossActionState::None;
    bool jumpRequested = false;
};