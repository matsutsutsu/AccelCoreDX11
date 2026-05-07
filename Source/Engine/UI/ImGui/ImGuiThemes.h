#pragma once

// ===========================================================================
// File: ImGuiThemes.h / .cpp
//
// 【役割】デバッグ用UIツール「ImGui」のカラーテーマ設定
//
// 【解説】
// 開発者用デバッグウィンドウ（ImGui）の見た目（色使い）を変更する関数群です。
// Unity風、Visual Studio風などのプリセットを用意しており、
// 開発中の気分転換や視認性向上のために使われます。
// ※ゲーム本編のUIには影響しません。
// ===========================================================================

namespace ImGuiThemes
{
    void ApplyDark();
    void ApplyDarcula();
    void ApplySkeltonDarcula();
    void ApplyLight();
    void ApplyClassic();

    void ApplyUnityDark();
    void ApplyVisualStudioDark();

}
