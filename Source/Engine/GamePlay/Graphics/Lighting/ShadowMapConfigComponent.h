#pragma once
#include <DirectXMath.h>

struct ShadowMapConfigComponent {
    float shadowBias = 0.0015f;
    DirectX::XMFLOAT4 shadowColor = { 0.0f, 0.0f, 0.0f, 0.5f }; // 影の色（濃さ）

	// CSMの各層がどこまでカバーするかの値（カメラからの距離）。例: { 15.0f, 50.0f, 200.0f }
    DirectX::XMFLOAT3 cascadeSplits = { 15.0f, 50.0f, 200.0f };
};