// EditorWindowManager.h
#pragma once
#include <vector>
#include <memory>
#include "EditorWindow.h"

class EditorWindowManager {
public:
    EditorWindowManager() = default;

	// ウィンドウの初期化と登録
    void Initialize();

    // 全ウィンドウの一括描画
    void Draw(EditorContext &context);

    bool IsVisible() const { return _isVisible; }

private:

    void DrawMainMenuBar(EditorContext& context);

    // ウィンドウの登録
    template<typename T, typename... Args>
    T* RegisterWindow(Args&&... args) {
        auto window = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = window.get();
        _windows.push_back(std::move(window));
        return ptr; // ★ポインタを返すことで、登録した直後に設定をいじれるようになる
    }

    std::vector<std::unique_ptr<EditorWindow>> _windows;

    // UI全体の表示フラグ
    bool _isVisible = true;
};