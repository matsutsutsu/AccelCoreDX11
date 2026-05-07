#pragma once
//4/15作製　桃田

#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

// 1. 状態の定義
enum class PlayerState : uint8_t
{
    Idle,
    Walking,
    Running
};


// 3. FPSプレイヤーの能力・データ
struct FPSPlayerComponent 
{
    // 1. まず固定サイズの単純な数値を固める（アライメントが安定する）
    CCL::ECS::EntityID physicsBodyID = CCL::ECS::InvalidEntityID;
    PlayerState currentState = PlayerState::Idle;
    float stateTimer = 0.0f;

    //速度設定
    float acceleration = 4.0;
    float walkSpeed = 3.0f;
    float runSpeed = 8.0f;
    //リアルタイムに変更される速度
    float currenTargetSpeed = 0.0f;
    float currentSpeed = 0.0f;

    //方向による速度減衰率
    float sideSpeedModifier = 0.8f;  // 真横移動時の速度 (80%)
    float backSpeedModifier = 0.6f;  // 後退時の速度 (60%)

    // 2. 構造体も、中身が float だけなら上に固めてもOK
    struct InputData {
        float moveForward = 0.0f;
        float moveRight = 0.0f;
        bool isRunPressed = false;
        bool isHidePressed = false; // 追加
        bool isHideHolding = false; // 追加
       
        // boolの後にはパディングが入る可能性があるので注意
    } input;

    // 3. std::string などの可変サイズ／複雑なオブジェクトは一番最後に置く
    struct InputLibrary 
    {
        struct Axis 
        {
            std::string moveFB = "MoveFB";
            std::string moveLR = "MoveLR";
        } axis;
        struct Action 
        {
            std::string dash = "Dash";
            std::string hide = "Hide";
        } action;
    } config;

};