#pragma once
#include "ECS/Common/CCL_Common.h"

// ダメージを受けた瞬間にだけエンティティに付与されるコンポーネント
struct HitFlashComponent {
    float duration     = 0.1f; // フラッシュする時間（0.1秒程度が気持ちいいです）
    float timer        = 0.1f; // 現在の残り時間
    float maxIntensity = 0.8f; // フラッシュの強さ（1.0で完全に真っ白）
};