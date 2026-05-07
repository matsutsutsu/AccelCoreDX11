#pragma once
#include "ECS/Common/CCL_Common.h"

// ECS限界テスト用のコンポーネント
struct OrbitBenchmarkComponent {
    float angle;      // 現在の角度
    float orbitSpeed; // 回る速さ
    float radius;     // 中心からの距離
    float waveSpeed;  // 上下に波打つ速さ
};