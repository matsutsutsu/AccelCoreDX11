// EditorWindow.h
#pragma once
#include <string>
#include <imgui.h>
#include "EditorContext.h"

// エディタウィンドウの基底クラス
class EditorWindow {
public:
    EditorWindow(const std::string& name, bool defaultVisible = true) : _name(name), _isVisible(defaultVisible) {}
    virtual ~EditorWindow() = default;

    // WindowManagerから呼ばれる描画の入り口
    void Render(EditorContext& context) {
        if (!_isVisible) return;

        if (ImGui::Begin(_name.c_str(), &_isVisible)) {
            DrawContents(context);
        }
        ImGui::End();
    }

    const std::string& GetName() const { return _name; }
    bool GetVisible() const { return _isVisible; }
    void SetVisible(bool open) { _isVisible = open; }

protected:
    // 各ウィンドウはこの関数をオーバーライドして中身を書く
    virtual void DrawContents(EditorContext& context) = 0;

    // クラスじゃなくてコンテキストにしてもいいかも

    std::string _name;
    bool _isVisible;
};