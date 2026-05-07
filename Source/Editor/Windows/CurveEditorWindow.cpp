#include "CurveEditorWindow.h"
#include "Engine/Platform/Logger.h"
#include "Engine/Platform/Dialog.h" // ★ダイアログ機能
#include "Engine/Graphics/Core/Graphics.h" // ★ウィンドウハンドル取得用
#include <fstream>
#include <imgui.h>
#include <filesystem> // ★パス変換用

using json = nlohmann::json;
namespace fs = std::filesystem;

// このIDを増やすことで、ImCurveEditの内部カメラを強制リセットします
static int s_curveEditorId = 1;

CurveEditorWindow::CurveEditorWindow() : EditorWindow("Curve Editor") {
    _editingCurve.name = "NewCurve";
    _editingCurve.keys.push_back({ 0.0f, 1.0f });
    _editingCurve.keys.push_back({ 1.0f, 1.0f });
}

void CurveEditorWindow::DrawContents(EditorContext& context) {
    // ==============================================================
    // 1. ツールバー (ダイアログ連携)
    // ==============================================================
    if (ImGui::Button("Load...")) {
        char filename[MAX_PATH] = {};
        // Windows標準のファイル開くダイアログを表示
        auto result = Dialog::OpenFileName(
            filename,
            MAX_PATH,
            "JSON Files\0*.json\0",
            "Select Curve File",
            Graphics::Instance().GetWindowHandle()
        );

        if (result == DialogResult::OK) {
            // 絶対パスをエンジン相対パスに変換する (保守性のための必須処理)
            fs::path absPath = filename;
            fs::path currentPath = fs::current_path();
            std::error_code ec;
            fs::path relPath = fs::relative(absPath, currentPath, ec);

            _currentFilePath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;
            LoadCurve(_currentFilePath);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Save As...")) {
        char filename[MAX_PATH] = {};
        // ファイル保存ダイアログを表示
        auto result = Dialog::SaveFileName(
            filename,
            MAX_PATH,
            "JSON Files\0*.json\0",
            "Save Curve File",
            "json",
            Graphics::Instance().GetWindowHandle()
        );

        if (result == DialogResult::OK) {
            fs::path absPath = filename;
            fs::path currentPath = fs::current_path();
            std::error_code ec;
            fs::path relPath = fs::relative(absPath, currentPath, ec);

            _currentFilePath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;
            SaveCurve(_currentFilePath);
        }
    }

    ImGui::Separator();

    // ====================================================================
    // グラフの表示がおかしくなった時に直すためのリセットボタン
    // ====================================================================
    if (ImGui::Button("Reset Graph View")) {
        s_curveEditorId++; // IDを変更して内部状態をフラッシュ
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("グラフの右半分が空白になる等、表示が崩れた際に視点を初期化します。");
    }

    // 2. 現在のファイル情報とカーブ名の表示
    ImGui::Text("File: %s", _currentFilePath.empty() ? "None" : _currentFilePath.c_str());

    char nameBuf[64];
    strcpy_s(nameBuf, _editingCurve.name.c_str());
    if (ImGui::InputText("Curve Name", nameBuf, sizeof(nameBuf))) {
        _editingCurve.name = nameBuf;
    }

    ImGui::Separator();

    // 3. カーブ編集キャンバス
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 300);
    CurveDelegate delegate(&_editingCurve);

    // 3. カーブ編集キャンバス
    ImGui::Text("X: Progress (0.0 to 1.0)  /  Y: Speed Multiplier");
    ImGui::TextDisabled("[Tip] Middle-Click Drag to Pan, Scroll Wheel to Zoom");

    // ====================================================================
    // ★修正: 縦軸と横軸の意味をカラーテキストで明確化し、背景を暗くして境界をくっきりさせる
    // ====================================================================
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "X軸 (横): アニメーション進捗率 (0.0 = 開始時, 1.0 = 終了時)");
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Y軸 (縦): 再生速度の倍率 (1.0 = 等速, 2.0 = 2倍速, 0.0 = 停止)");
    ImGui::TextDisabled("[操作] ホイール:ズーム / 中ボタンドラッグ:パン");

    ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 250);
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f)); 
    if (ImGui::BeginChild("CurveCanvas", canvasSize, true)) {
        CurveDelegate delegate(&_editingCurve);
        
        // ★修正: グラフの限界値を厳密に定義。縦軸(Y)は最大2.0倍速を基本の天井とする
        delegate.m_min = ImVec2(0.0f, 0.0f);
        delegate.m_max = ImVec2(1.0f, 2.0f); 

        // IDに s_curveEditorId を渡すことで、リセット機能が働くようにする
        ImCurveEdit::Edit(delegate, ImGui::GetContentRegionAvail(), s_curveEditorId);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();

    // ====================================================================
    // 　キーフレームの正確な数値をリスト表示して直接編集可能にする
    // ====================================================================
    ImGui::Text("Keyframes (Exact Values)");
    ImGui::Indent();

    for (size_t i = 0; i < _editingCurve.keys.size(); ++i) {
        ImGui::PushID((int)i);

        // 削除ボタン (最低2点は残す)
        if (_editingCurve.keys.size() > 2) {
            if (ImGui::Button("X")) {
                _editingCurve.keys.erase(_editingCurve.keys.begin() + i);
                ImGui::PopID();
                break; // 配列が変わったのでループを抜ける
            }
            ImGui::SameLine();
        }
        else {
            ImGui::Text(" "); ImGui::SameLine(); // 削除不可時のスペーサー
        }

        ImGui::Text("Key %d:", (int)i);
        ImGui::SameLine();

        float t = _editingCurve.keys[i].time;
        float v = _editingCurve.keys[i].value;

        // X軸（時間）の編集。0.0 ~ 1.0 の範囲を強制
        ImGui::SetNextItemWidth(120);
        if (ImGui::DragFloat("Time", &t, 0.01f, 0.0f, 1.0f, "%.3f")) {
            _editingCurve.keys[i].time = t;
        }

        ImGui::SameLine();

        // Y軸（速度・値）の編集。マイナス方向も設定可能にしておく
        ImGui::SetNextItemWidth(120);
        if (ImGui::DragFloat("Value", &v, 0.05f, -5.0f, 10.0f, "%.3f")) {
            _editingCurve.keys[i].value = v;
        }

        ImGui::PopID();
    }
    ImGui::Unindent();

    // 手動でポイントを追加するボタン
    if (ImGui::Button("+ Add Keyframe")) {
        // デフォルトで 0.5 の位置に 1.0 の値を追加
        _editingCurve.keys.push_back({ 0.5f, 1.0f });
    }

    // ★重要: 値を書き換えた後、時間(X軸)順にソートし直すことでグラフが破綻しないようにする
    std::sort(_editingCurve.keys.begin(), _editingCurve.keys.end(),
        [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
}

// -------------------------------------------------------------
// セーブ＆ロード処理
// -------------------------------------------------------------
void CurveEditorWindow::SaveCurve(const std::string& path) {
    std::ofstream os(path);
    if (!os.is_open()) {
        CCL_LOG_ERROR(LogCategory::Editor, "Failed to save curve: %s", path.c_str());
        return;
    }
    json j = _editingCurve;
    os << j.dump(4);
    CCL_LOG_SUCCESS(LogCategory::Editor, "Curve saved to %s", path.c_str());
}

void CurveEditorWindow::LoadCurve(const std::string& path) {
    std::ifstream is(path);
    if (!is.is_open()) {
        CCL_LOG_ERROR(LogCategory::Editor, "Failed to load curve: %s", path.c_str());
        return;
    }

    try {
        json j;
        is >> j;
        _editingCurve = j.get<AnimationCurve>();
        CCL_LOG_SUCCESS(LogCategory::Editor, "Curve loaded from %s", path.c_str());
    }
    catch (const std::exception& e) {
        CCL_LOG_ERROR(LogCategory::Editor, "Failed to parse curve JSON: %s", e.what());
    }
}