#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

enum class PrimitiveType { Box, Sphere, Cylinder, Capsule, Cone };

struct PrimitiveComponent {
    PrimitiveType type = PrimitiveType::Box;

    // 形状ごとのパラメータ（共用してメモリ節約してもいいですが、分かりやすさ優先で羅列します）
    DirectX::XMFLOAT3 size   = {1.0f, 1.0f, 1.0f}; // Box用
    float             radius = 0.5f;               // Sphere, Cylinder, Capsule, Cone用
    float             height = 1.0f;               // Cylinder, Capsule, Cone用

    DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 色

    bool isWireframe = false; // ワイヤーフレームで描画するか
};