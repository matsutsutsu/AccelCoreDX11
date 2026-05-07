// HidingComponents.h
#pragma once
#include <DirectXMath.h>
#include "ECS/Common/CCL_Common.h"

namespace HidingSpotTag
{
    //机やベッド等に付けます下から潜るような軌道で移動
    struct Under
    {
        float sinkDepth = 0.0f;      // どのくらい深く沈むか
        float lookDownAngle = 10.0f; // 潜る時にどれだけ下を向くか
    };

    //ロッカーやタンス等に付けますそれらを開けるモーションの待機をしてから移動
    struct Open
    {
        float waitTime = 1.0f;       // 扉を開ける等の待機時間
    };

    //ドラム缶等につけます視点を少し上に移動してからその中に入るような軌道で移動
    struct TopIn
    {
        float riseHeight = 0.4f;    // どのくらい上に移動するか（縁までの高さ）
        float lookDownAngle = 30.0f; // 覗き込む時に下を向く角度
        float sinkDepth = 0.1f;     // 最終的に中に沈む深さ
    };
};

// 隠れ場所オブジェクトに付与
struct HidingSpotComponent 
{
    // 隠れた時のプレイヤーの相対位置
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    // 出る時のプレイヤーの相対位置（例：ロッカーの少し前など）
    DirectX::XMFLOAT3 exitOffset = { 0.0f, 0.0f, 1.5f };
    bool isOccupied = false;

    // Under: 負の値, TopIn: 正の値 を設定
    float trajectoryArc = 0.0f;
    // アニメーションの「溜め」時間（Openタグ用：扉を開ける時間など）
    float prepTime = 0.0f;

    // --- カメラ・移動の補間設定 ---
    float camerarotationSpeed = 5.0f;  // 潜伏時の回転速度
    float exitRotationSpeed = 8.0f;    // 脱出時の回転速度
    float translationSpeed = 0.75f;     // 位置移動の速度
    float targetYawOffset = 180.0f;    // 潜伏完了時の回転量
};