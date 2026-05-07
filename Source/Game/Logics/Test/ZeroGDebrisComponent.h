#pragma once
#include <SimpleMath.h>
#include <cstdint>

/**
 * @brief 無重力空間で波紋のように上下に浮遊する瓦礫のデータ
 * @note 実行時の計算コストを最小化するため、初期化時に計算できる値（baseY, distanceFromCenter）は事前にキャッシュしておく。
 */
struct ZeroGDebrisComponent {
    float baseY = 0.0f;
    float distanceFromCenter = 0.0f;

    float floatAmplitude = 1.0f;
    float floatFrequency = 1.0f;
    float waveSpeed = 2.0f;

    DirectX::SimpleMath::Vector3 rotationAxis = { 0, 1, 0 };
    float rotationSpeed = 1.0f;
    float timeAcc = 0.0f; // 個体ごとの経過時間
};