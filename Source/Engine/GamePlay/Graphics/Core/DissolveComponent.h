// Game/Logics/Combat/DissolveComponent.h
#pragma once

struct DissolveComponent {
    float currentThreshold = 0.0f; // 現在のディゾルブ進行度 (0.0=完全な姿, 1.0=消滅)
    float dissolveSpeed    = 1.0f; // 1秒で消えるなら1.0、2秒なら0.5
};