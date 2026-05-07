#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>



struct TPSPlayerComponent 
{

    float walkSpeed = 10.0f;
    float runSpeed = 15.0f;
    float currentSpeed = 0.0f;
    float rotationSpeed = 15.0f; // キャラが進行方向を向く速度
    float accel = 20.0f; // 加速度

    // 入力生データ
    struct {
        DirectX::XMFLOAT2 moveInput; // x: Right, y: Forward
        bool isSprintHeld = false;
        bool isDodgeTriggered = false;
        bool isAttackTriggered = false;
        bool isGuardHeld = false;
        bool isJumpTriggered = false;
        bool isTargeting = false;
        bool isTargetDecided = false;
        bool isLockOnTriggered = false;
        bool isLockOnChengeTriggered = false;
        float dodgeTimer;
    } input;

    // アクション制御（Systemがタイマーを更新）
    float stateTimer = 0.0f;
    float dodgeCooldown = 0.5f;
    float lastDodgeTime = -1.0f;

    float jumpImpulse = 8.0f;     // ジャンプ初速
    int jumpCount = 0;            // 現在のジャンプ回数
    int maxJumpCount = 2;         // 最大ジャンプ回数（2段ジャンプなら2）
    bool isGrounded = false;      // 接地状態
    bool forceStopMovement = false; // 攻撃終了時などに速度を強制ゼロにする用
    bool hasPerformedAirAttack = false;//空中攻撃済みフラグ
    float groundRayLength = 0.2f; // 接地判定のレイの長さ（キャラの足元から）
    bool canMove = true;          //今現在移動可能かを判別
    float moveRate = 1.0f;          //今現在移動レートの表示

    // メンバ変数などに追加
    float jumpIgnoreTimer = 0.0f;
    float JUMP_IGNORE_DURATION = 0.15f; // 0.15秒間は着地判定を無視
    // 他のシステム（Dodge等）から一時的に与えられる追加速度
    // 毎フレーム MoveSystem の最後でリセットされる
    DirectX::XMFLOAT3 externalVelocity = { 0, 0, 0 };
    // 球体の中心に対して、モデルの足元を合わせるためのオフセット
    DirectX::XMFLOAT3 visualOffset = { 0.0f, -1.0f, 0.0f };

    // 設定
    struct InputLibrary
    {
        struct Axis
        {
            std::string moveFB = "MoveFB";
            std::string moveLR = "MoveLR";
        }axis;
        struct Action
        {
            std::string sprint = "Sprint";
            std::string dodge = "Dodge";
            std::string attack = "Attack";
            std::string decide = "Decide"; 
            std::string Targeting = "Targeting";
            std::string guard = "Guard";
            std::string jump = "Jump";
            std::string lockOn = "lockOn";
            std::string lockOnChenge = "LockOnChenge";
        }action;
    } config;
};
