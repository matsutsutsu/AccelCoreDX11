#pragma once
//4/17--桃田--追加
// --- FPSPlayerViewComponent.h ---
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

//FPSPlayerがカメラに対して変更を加えるときはこちらで管理する
struct FPSPlayerViewComponent 
{
    // シリアライズ対象 (固定値)
    float baseFOV = 60.0f;
    float runFOV = 80.0f;
    float fatigueFOV = 45.0f;       // 疲労時（視界が狭まり、圧迫感を出す）
    float fovInterpSpeed = 10.0f;

    // 実行時データ (シリアライズ不要)
    float currentFOV = 60.0f;

    // インスペクターで設定する「基本の目の高さ」
    DirectX::XMFLOAT3 baseEyeOffset = { 0.0f,0.7f, 0.0f };

    // --- Bobbing設定 (カメラの揺れ) ---

        // 待機(Idle)時の呼吸を模した微細な揺れ
    float idleBobSpeed = 2.5f;     // 揺れる速さ (呼吸のピッチ)
    float idleBobAmount = 0.015f;   // 揺れる大きさ (上下の深さ)
    // 歩行(Walk)時の一歩ごとの揺れ
    float walkBobSpeed = 8.0f;     // 歩行周期に合わせた揺れの速さ
    float walkBobAmount = 0.04f;   // 歩行時の頭の上下動の幅
    // 走行(Run)時の激しい揺れ
    float runBobSpeed = 15.0f;      // 走行周期に合わせた高速な揺れ
    float runBobAmount = 0.08f;    // 全力疾走時の大きな上下動の幅
    // 走行時の演出効果
    float runTiltAmount = 0.02f;   // 走行中に左右に踏み込む際のカメラの傾き(Roll)の最大幅

    // 内部計算用
    float bobTimer = 0.0f;
};