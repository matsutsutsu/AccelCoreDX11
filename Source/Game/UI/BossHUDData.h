#pragma once

// ボスUIへのデータ受け渡し用リソース
struct BossHUDData {
    float hpRatio = 1.0f; // ボスのHP割合 (0.0 ~ 1.0)
    bool isVisible = false; // ボス戦中かどうか（UIを出すか消すか）
    int currentPhase = 1; // フェーズ（演出用）
};