#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

//player特有の状態変化をここにまとめます
namespace PlayerTag
{
    enum class HideState {
        Entering, // 潜伏場所へ移動中
        Hiding,   // 潜伏中
        Exiting   // 脱出場所へ移動中
    };
    //これを所持している場合隠れている状態とみなす
    struct HideTag {
        CCL::ECS::EntityID spotEntity;   // どのオブジェクトに隠れているか
        DirectX::XMFLOAT3 originalPos;   // 出る時に戻るためのワールド座標

        HideState state = HideState::Entering;
        float lerpT = 0.0f;            // 位置補間用
        bool isRotating = false;           // カメラ回転中か
        float initialYaw = 0.0f;           // 回転開始時のYaw
        float targetYaw = 0.0f;            // 目標のYaw
        float rotationT = 0.0f;            // 回転の進行度
    };
}
 
