#pragma once

#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

// StaminaComponent.h
struct StaminaComponent
{
    // --- 現在の状態 (Dynamic) ---
    float current = 100.0f;           // 現在のスタミナ値
    bool isFatigued = false;          // 疲労状態フラグ
    bool isConsuming = false;         // 消費中フラグ (MoveSystem等から通知)
    bool isMoving = false;            // 移動中フラグ (MoveSystem等から通知)
    float recoveryDelayTimer = 0.0f;  // 回復開始までの待機タイマー

    // システム外（回避処理等）からこの値に消費量を加算します
    float instantConsumeRequest = 0.0f;

    // --- 基本パラメータ (Base Settings) ---
    float maxStamina = 100.0f;               // 最大スタミナ
    float recoveryRateBase = 5.0f;           // 1秒あたりの基本回復量
    float consumeRate = 2.0f;               // 1秒あたりの消費速度
    float recoveryDelayTime = 0.6f;          // 消費終了から回復開始までの秒数
    float moveRecoveryMultiplier = 0.75f;    // 移動中の回復倍率
    float fatigueRecoveryThreshold = 0.0001f;   // 最大値の30%まで回復したら疲労解除

    // --- 疲労デバフ設定 (Fatigue Modifier Values) ---
    // ※これらを変更することで、疲労時のペナルティ強度を調整可能
    float fatigueMoveSpeedMult = 0.7f;  // 移動速度低下率 (0.7 = 30%減)
    float fatigueRecoveryMultiplier = 0.5f;  // 回復速度低下率 (0.5 = 50%減)
    float fatigueFOVMult = 0.9f;           // FOV圧縮率

    // 停止(Idle)中の疲労演出
    float fatigueIdleBobAmountMult = 1.5f; // 肩で息をするような上下の揺れ
    float fatigueIdleBobSpeedMult = 0.8f;  // 呼吸なので少しゆっくりにする等の調整
    float fatigueIdleTiltMult = 1.2f;      // 立ちくらみのような揺れ

    // 歩行(Walk)中の疲労演出
    float fatigueWalkBobAmountMult = 2.0f; // 足元がふらつく大きな揺れ
    float fatigueWalkBobSpeedMult = 1.3f;  // 必死に歩くためのピッチ増
    float fatigueWalkTiltMult = 2.0f;      // 踏み込むたびに大きく傾く
};
