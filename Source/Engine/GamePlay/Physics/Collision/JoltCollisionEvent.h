#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

// Jolt空間からECS空間へ配達される「衝突の手紙」
struct JoltCollisionEvent {
    CCL::ECS::EntityID entityA;         // ぶつかった側
    CCL::ECS::EntityID entityB;         // ぶつかられた側
    DirectX::XMFLOAT3  contactPosition; // 衝突したワールド座標（火花を出す位置）
    DirectX::XMFLOAT3  contactNormal;   // 衝突面の法線（跳ね返る方向）
};