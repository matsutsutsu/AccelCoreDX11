#pragma once
#include <DirectXMath.h>

struct JoltBoxColliderComponent {
    // Joltの仕様に合わせ、最初から「半分のサイズ(Half-Extent)」で定義する
    DirectX::XMFLOAT3 halfExtent = { 0.5f, 0.5f, 0.5f };

    // 基準点からのローカル位置ズレ
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };

    // コライダー単体のローカル回転（度数法：Degree）
    DirectX::XMFLOAT3 localRotationEuler = { 0.0f, 0.0f, 0.0f };

    bool isDirty = false;
};