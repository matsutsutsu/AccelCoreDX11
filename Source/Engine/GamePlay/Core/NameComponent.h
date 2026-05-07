#pragma once
#include <cstring> // for strncpy_s

struct NameComponent {
    // 32文字あれば通常のゲームオブジェクト名としては十分です。
    // 必要なら 64 などに増やしても良いですが、キャッシュ効率のため小さい方が良いです。
    char name[32] = "Entity";

    // コンストラクタで初期化しやすくする
    NameComponent() = default;
    NameComponent(const char *str)
    {
        if (str) {
            // 安全にコピー（バッファオーバーラン防止）
            strncpy_s(name, str, sizeof(name) - 1);
        }
    }
};