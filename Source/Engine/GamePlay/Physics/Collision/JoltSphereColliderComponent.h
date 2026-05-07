#pragma once
#include <DirectXMath.h>

struct JoltSphereColliderComponent {
    float radius = 0.5f;

    // 基準点からのローカル位置ズレ
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };

    bool isDirty = false;
};