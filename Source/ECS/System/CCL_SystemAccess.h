#pragma once

namespace CCL::ECS {
    // --------------------------------------------------------
    // Read: 読み込み専用アクセス
    // --------------------------------------------------------
    template <typename T> struct Read {
        using RawType                 = T;         // 実際のコンポーネント型
        using ParamType               = const T &; // ループ内で受け取る型（const参照）
        static constexpr bool IsWrite = false;
    };

    // --------------------------------------------------------
    // Write: 書き込み可能アクセス
    // --------------------------------------------------------
    template <typename T> struct Write {
        using RawType                 = T;   // 実際のコンポーネント型
        using ParamType               = T &; // ループ内で受け取る型（非const参照）
        static constexpr bool IsWrite = true;
    };
} // namespace CCL::ECS