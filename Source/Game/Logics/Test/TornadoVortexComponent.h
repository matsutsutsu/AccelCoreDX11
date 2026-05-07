#pragma once
#include <SimpleMath.h>

struct TornadoVortexComponent {
    float radius;         // 現在の中心からの距離
    float angle;          // 現在の角度（ラジアン）
    float height;         // 現在の高さ

    float shrinkSpeed;    // 中心へ向かう速度
    float rotationSpeed;  // 回転速度
    float riseSpeed;      // 上昇速度

    float maxRadius;      // リセット用の最大半径
    DirectX::SimpleMath::Vector3 centerPos; // 竜巻の中心底面
};