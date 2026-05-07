#pragma once
#include "ECS/Common/CCL_Common.h"

// ボクセルオーシャン用のコンポーネント
struct VoxelOceanComponent {
    float timeElapsed = 0.0f; // 経過時間
    float waveSpeed = 1.5f; // 波がうねる全体的なスピード
};