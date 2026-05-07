#pragma once
#include <DirectXMath.h>
#include <string>

struct FloatingTextComponent {
    std::string       text          = "Check";            // 表示する文字
    float             triggerRadius = 3.0f;               // 反応する距離
    DirectX::XMFLOAT3 offset        = {0.0f, 2.0f, 0.0f}; // 頭上などに表示するための位置オフセット

    // ★追加: 中央揃えにするかどうか (デフォルトは true)
    bool centerAlign = true;

    // 内部管理用（触らなくてOK）
    bool        isShowing = false; // 現在表示中か？
    std::string uiName    = "";    // TextManager登録用のユニーク名
};