#pragma once

struct BloomConfigComponent {
    bool enable = true;
    float threshold = 1.0f;   // これ以上の明るさを光らせる（抽出閾値）
    float intensity = 1.0f;   // 光の強さ
    float softKnee = 0.5f;    // 閾値付近の滑らかさ
    float radius = 1.0f;      // ぼかしの広がり係数
};