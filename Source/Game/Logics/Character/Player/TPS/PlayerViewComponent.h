#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

//FPSPlayerがカメラに対して変更を加えるときはこちらで管理する
struct PlayerViewComponent
{
    // --- FOV 設定 ---
    float baseFOV = 60.0f;  // 通常時の基本視野角
    float currentFOV = 60.0f;  // 現在の計算済み視野角 (補間用)
    float dodgeUpFov = 20.0f;  // 回避/ダッシュ時に加算するFOV量
    float fovInterpSpeed = 15.0f;  // FOVの変化速度 (lerp用)

    // --- カメラ揺れ (Bobbing) 設定 ---
    float bobTimer = 0.0f;

    float idleBobSpeed = 1.0f;
    float idleBobAmount = 0.05f;

    float walkBobSpeed = 8.0f;
    float walkBobAmount = 0.15f;

    float runBobSpeed = 12.0f;
    float runBobAmount = 0.25f;
};