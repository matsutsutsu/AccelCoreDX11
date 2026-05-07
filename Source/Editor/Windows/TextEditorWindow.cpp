#include "TextEditorWindow.h"

#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Dialog.h"
#include "Engine/Graphics/Renderer/ImGuiRenderer.h"

#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

TextEditorWindow::TextEditorWindow() : EditorWindow("Text Editor")
{
    // デフォルトパスの初期化
    //strcpy_s(_filePath, sizeof(_filePath), "Assets/Weapon/AttachmentMaster.json");
    strcpy_s(_filePath, sizeof(_filePath), "Shader/Material/Phong/PhongPS.hlsl");

    // 編集用のメモリバッファを大きめに確保（例：1MB = 1024 * 1024 バイト）
    // これだけあれば数万行のシェーダーコードでも余裕で収まります。
    _textBuffer.resize(1024 * 1024, '\0');
}

void TextEditorWindow::DrawContents(EditorContext &context)
{
    // ---------------------------------------------------------
    // ① ファイルパスと操作ボタン領域
    // ---------------------------------------------------------
    ImGui::InputText("File Path", _filePath, sizeof(_filePath));
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        char filename[256] = {0};
        HWND hWnd          = Graphics::Instance().GetWindowHandle();

        // 対応フォーマットを増やしたフィルタ
        const char *filter = "Supported Files\0*.json;*.lua;*.cpp;*.h;*.hlsl;*.hlsli\0"
                             "HLSL Shaders\0*.hlsl;*.hlsli\0"
                             "JSON Files\0*.json\0"
                             "Lua Scripts\0*.lua\0"
                             "All Files\0*.*\0";

        if (Dialog::OpenFileName(filename, 256, filter, "Select File to Edit", hWnd) ==
            DialogResult::OK) {
            strcpy_s(_filePath, sizeof(_filePath), filename);
            LoadFile(_filePath);
        }
    }

    ImGui::Separator();

    // 操作ボタン群
    if (ImGui::Button("Load")) {
        LoadFile(_filePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveFile(_filePath);
    }
    ImGui::SameLine();

    // ★ ホットリロードボタン（ファイルに応じて処理を分ける）
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("Apply (Save & Reload)")) {
        SaveFile(_filePath);

        // ファイルパスを文字列として判定
        std::string pathStr(_filePath);

      
        // Luaスクリプトの場合の処理（必要になれば拡張してください）
        // else if (pathStr.find(".lua") != std::string::npos) {
        //     // TODO: Lua VMをリロードしたり、コンポーネントに再読み込み指令を出す
        // }
    }
    ImGui::PopStyleColor();

    ImGui::Separator();

    // ---------------------------------------------------------
    // ② テキストエディタ領域 (ImGui標準)
    // ---------------------------------------------------------
    // ウィンドウの残りの「縦横サイズ」を計算
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    // =========================================================
    // ここから下をエディタ専用フォント(Ricty)に切り替える！
    // =========================================================
    if (ImFont *editorFont = ImGuiRenderer::GetEditorFont()) {
        ImGui::PushFont(editorFont);
    }

    // ImGui標準の複数行テキストボックスを描画！
    // ImGuiInputTextFlags_AllowTabInput を付けることで、Tabキーで空白が入力できるようになります。
    ImGui::InputTextMultiline("##TextEditor",
        _textBuffer.data(),
        _textBuffer.size(),
        availableSize,
        ImGuiInputTextFlags_AllowTabInput);


    // =========================================================
    // フォントの切り替えを元に戻す！
    // =========================================================
    if (ImGuiRenderer::GetEditorFont()) {
        ImGui::PopFont();
    }
}

void TextEditorWindow::LoadFile(const char *path)
{
    std::ifstream ifs(path);
    if (ifs.is_open()) {
        std::stringstream ss;
        ss << ifs.rdbuf();

        // ファイルから読み込んだ文字列
        std::string rawText = ss.str();

        // もし読み込んだファイルがバッファ(1MB)よりデカかったら、バッファを拡張する安全設計
        if (rawText.size() >= _textBuffer.size()) {
            _textBuffer.resize(rawText.size() * 2, '\0');
        }

        // 古い文字列のゴミが残らないように、バッファ全体をゼロ('\0')でクリア
        std::fill(_textBuffer.begin(), _textBuffer.end(), '\0');

        // バッファに文字列をコピー
        std::copy(rawText.begin(), rawText.end(), _textBuffer.begin());
    }
}

void TextEditorWindow::SaveFile(const char *path)
{
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        // バッファの中身をそのままC文字列としてファイルに書き出す
        ofs << _textBuffer.data();
    }
}