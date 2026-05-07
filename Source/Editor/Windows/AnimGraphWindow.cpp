#include "AnimGraphWindow.h"
#include "Engine/GamePlay/Animation/Data/AnimGraphSerializer.h"
#include "Engine/Core/Math/StringHash.h"
#include <imgui_node_editor.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>

#include "Engine/Platform/Dialog.h"
#include "Engine/Platform/Logger.h"
#include "Engine/Graphics/Core/Graphics.h"
#include <filesystem>

namespace ed = ax::NodeEditor;

const uint32_t ENTRY_NODE_ID = 0x0000000A; // 10
const uint32_t ENTRY_OUT_PIN = 0x0000000B; // 11
const int ENTRY_LINK_ID = 0x0000000C; // 12

AnimGraphWindow::AnimGraphWindow() : EditorWindow("Anim Graph Editor") {
    _nodeContext = ed::CreateEditor();
}

AnimGraphWindow::~AnimGraphWindow() {
    if (_nodeContext) ed::DestroyEditor(_nodeContext);
}

void AnimGraphWindow::DrawContents(EditorContext& context) {
    ed::SetCurrentEditor(_nodeContext);
    DrawToolbar();

    // カラム幅の強制上書きを初回のみにし、ユーザーが境界線をドラッグできるようにする
    static bool initColumns = true;
    ImGui::Columns(2, "AnimGraphColumns");
    if (initColumns) {
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.75f);
        initColumns = false;
    }

    // ノードエディタの「仮想座標系」が漏れ出さないよう、ChildWindowで完全にカプセル化する
    ImGui::BeginChild("NodeWorkspaceChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawNodeWorkspace();
    ImGui::EndChild();

    ImGui::NextColumn();

    // インスペクタもChildWindowにし、独立したスクロール領域と安全な座標系を持たせる
    ImGui::BeginChild("InspectorChild", ImVec2(0, 0), false);
    DrawInspector();
    ImGui::EndChild();

    ImGui::Columns(1);
}

void AnimGraphWindow::DrawNodeWorkspace()
{
    ed::Begin("My Anim Node Editor");

    ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);

    constexpr float NODE_WIDTH = 150.0f;

    // ================================================================
    // 1. Entry ノードの描画
    // ================================================================
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.3f, 0.2f, 0.1f, 1.0f));
    ed::BeginNode(ENTRY_NODE_ID);
    
    ImGui::Dummy(ImVec2(NODE_WIDTH, 5));
    float ew = ImGui::CalcTextSize("ENTRY").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - ew) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "ENTRY");
    ImGui::Dummy(ImVec2(NODE_WIDTH, 5));

    ImGui::Separator();

    // ★修正: Buttonを排除し、図形描画で「線を引けるダミーボタン」を作成
    ed::BeginPin(ENTRY_OUT_PIN, ed::PinKind::Output);
    ImVec2 cursorPosE = ImGui::GetCursorScreenPos();
    ImDrawList* dlE = ImGui::GetWindowDrawList();
    dlE->AddRectFilled(cursorPosE, ImVec2(cursorPosE.x + NODE_WIDTH, cursorPosE.y + 24), IM_COL32(204, 102, 25, 200), 4.0f);
    
    ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
    float twE = ImGui::CalcTextSize("● 線を伸ばす (Start)").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twE) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "● 線を伸ばす (Start)");
    ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
    ed::EndPin();

    ed::EndNode();
    ed::PopStyleColor();

    // ロード直後なら、保存されていた座標に強制移動させる
    if (_needRestorePositions) {
        ed::SetNodePosition(ENTRY_NODE_ID, ImVec2(_currentGraph.entryPosX, _currentGraph.entryPosY));
    }

    // ================================================================
    // 2. 通常ステートノードの描画
    // ================================================================
    for (auto& state : _currentGraph.states)
    {
        // ★修正: 画像の赤いエラー（ID衝突）を完全に防ぐためのPushID
        ImGui::PushID(state.stateHash); 

        const bool isEntry = (_currentGraph.entryStateHash == state.stateHash);
        ed::PushStyleColor(ed::StyleColor_NodeBg, isEntry ? ImVec4(0.15f, 0.3f, 0.15f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

        ed::BeginNode(state.stateHash);

        // --- 上部層: 遷移を受け入れるエリア (Input Pin) ---
        ed::BeginPin(state.stateHash + 1, ed::PinKind::Input);
        ImVec2 cursorPosI = ImGui::GetCursorScreenPos();
        ImDrawList* dlI = ImGui::GetWindowDrawList();
        dlI->AddRectFilled(cursorPosI, ImVec2(cursorPosI.x + NODE_WIDTH, cursorPosI.y + 24), IM_COL32(51, 153, 51, 200), 4.0f);
        
        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        float twI = ImGui::CalcTextSize("▼ ドロップで繋ぐ (In)").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twI) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "▼ ドロップで繋ぐ (In)");
        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        ed::EndPin();

        ImGui::Separator();

        // --- 中央層: ノード移動エリア ---
        ImGui::Dummy(ImVec2(NODE_WIDTH, 8));
        float tw = ImGui::CalcTextSize(state.sequenceName.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - tw) * 0.5f);
        ImGui::TextUnformatted(state.sequenceName.c_str());
        ImGui::Dummy(ImVec2(NODE_WIDTH, 8));

        ImGui::Separator();

        // --- 下部層: 線を伸ばすエリア (Output Pin) ---
        ed::BeginPin(state.stateHash + 2, ed::PinKind::Output);
        ImVec2 cursorPosO = ImGui::GetCursorScreenPos();
        ImDrawList* dlO = ImGui::GetWindowDrawList();
        dlO->AddRectFilled(cursorPosO, ImVec2(cursorPosO.x + NODE_WIDTH, cursorPosO.y + 24), IM_COL32(204, 102, 25, 200), 4.0f);
        
        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        float twO = ImGui::CalcTextSize("● 線を伸ばす (Out)").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twO) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "● 線を伸ばす (Out)");
        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        ed::EndPin();

        ed::EndNode();
        ed::PopStyleColor();

        ImGui::PopID(); // ★戻す

        // ロード直後なら座標を復元
        if (_needRestorePositions) {
            ed::SetNodePosition(state.stateHash, ImVec2(state.nodePosX, state.nodePosY));
        }
    }

    if (_nodeToPlaceHash != 0) {
        ImVec2 c = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
        ed::SetNodePosition(_nodeToPlaceHash, ed::ScreenToCanvas(ImVec2(c.x + s.x * 0.5f, c.y + s.y * 0.5f)));
        ed::SelectNode(_nodeToPlaceHash);
        ed::NavigateToSelection(false, 0.5f);
        _nodeToPlaceHash = 0;
    }

    // ================================================================
    // 3. ネイティブの線をしっかり描画する
    // ================================================================
    if (_currentGraph.entryStateHash != 0) {
        ed::Link(ENTRY_LINK_ID, ENTRY_OUT_PIN, _currentGraph.entryStateHash + 1, ImVec4(1.0f, 0.6f, 0.0f, 1.0f), 2.5f);
    }

    int linkId = 0x00000100;
    for (const auto& state : _currentGraph.states) {
        for (const auto& trans : state.transitions) {
            ed::Link(linkId++, state.stateHash + 2, trans.targetStateHash + 1, ImVec4(0.8f, 0.8f, 0.8f, 1.0f), 2.5f);
        }
    }
    ed::PopStyleVar();

    // すべてのノードの位置復元が終わったら、フラグを折って全体をカメラに収める
    if (_needRestorePositions) {
        ed::NavigateToContent();
        _needRestorePositions = false;
    }

    // ================================================================
    // 4. 矢印を安全にオーバーレイ描画
    // ================================================================
    ed::Suspend();
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto drawArrow = [&](uint32_t srcId, uint32_t dstId, ImU32 col) {
            ImVec2 sP = ed::GetNodePosition(srcId), sS = ed::GetNodeSize(srcId);
            ImVec2 dP = ed::GetNodePosition(dstId), dS = ed::GetNodeSize(dstId);

            if (sS.x == 0 || dS.x == 0) return; // 初回レイアウト待機

            // 線の開始地点: ノードの「下部」の中央
            ImVec2 startCanvas = ImVec2(sP.x + sS.x * 0.5f, sP.y + sS.y);
            // 線の終了地点: ノードの「上部」の中央
            ImVec2 endCanvas = ImVec2(dP.x + dS.x * 0.5f, dP.y);

            ImVec2 dir = ImVec2(endCanvas.x - startCanvas.x, endCanvas.y - startCanvas.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len > 1.0f) {
                dir.x /= len; dir.y /= len;
                
                ImVec2 tipCanvas = ImVec2(endCanvas.x - dir.x * 6.0f, endCanvas.y - dir.y * 6.0f);
                ImVec2 tip = ed::CanvasToScreen(tipCanvas);

                float A = 14.0f;
                ImVec2 perp = ImVec2(-dir.y, dir.x);
                dl->AddTriangleFilled(
                    tip,
                    ImVec2(tip.x - dir.x * A + perp.x * A * 0.5f, tip.y - dir.y * A + perp.y * A * 0.5f),
                    ImVec2(tip.x - dir.x * A - perp.x * A * 0.5f, tip.y - dir.y * A - perp.y * A * 0.5f),
                    col);
            }
        };

        if (_currentGraph.entryStateHash != 0) {
            drawArrow(ENTRY_NODE_ID, _currentGraph.entryStateHash, IM_COL32(255, 153, 0, 255));
        }
        for (const auto& state : _currentGraph.states) {
            for (const auto& trans : state.transitions) {
                drawArrow(state.stateHash, trans.targetStateHash, IM_COL32(204, 204, 204, 255));
            }
        }
    }
    ed::Resume();

    HandleInteraction();

    ed::End();

    ed::NodeId selNode; ed::LinkId selLink;
    _selectedNodeHash = 0; _selectedLinkId = -1;
    if (ed::GetSelectedNodes(&selNode, 1) > 0)
        _selectedNodeHash = (uint32_t)(uintptr_t)selNode.Get();
    else if (ed::GetSelectedLinks(&selLink, 1) > 0)
        _selectedLinkId = (int)(uintptr_t)selLink.Get();
}

void AnimGraphWindow::HandleInteraction()
{
    if (ed::BeginCreate()) {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {
            if (startPinId && endPinId) {
                uint32_t startId = (uint32_t)(uintptr_t)startPinId.Get();
                uint32_t endId = (uint32_t)(uintptr_t)endPinId.Get();

                bool isStartInput = false;
                bool isEndInput = false;
                uint32_t srcHash = 0;
                uint32_t dstHash = 0;

                if (startId == ENTRY_OUT_PIN) {
                    isStartInput = false; srcHash = ENTRY_NODE_ID;
                } else {
                    for (auto& s : _currentGraph.states) {
                        if (startId == s.stateHash + 1) { isStartInput = true; srcHash = s.stateHash; break; }
                        if (startId == s.stateHash + 2) { isStartInput = false; srcHash = s.stateHash; break; }
                    }
                }

                if (endId == ENTRY_OUT_PIN) {
                    isEndInput = false; dstHash = ENTRY_NODE_ID;
                } else {
                    for (auto& s : _currentGraph.states) {
                        if (endId == s.stateHash + 1) { isEndInput = true; dstHash = s.stateHash; break; }
                        if (endId == s.stateHash + 2) { isEndInput = false; dstHash = s.stateHash; break; }
                    }
                }

                uint32_t finalSrc = isStartInput ? dstHash : srcHash;
                uint32_t finalDst = isStartInput ? srcHash : dstHash;

                if (isStartInput == isEndInput || finalDst == ENTRY_NODE_ID) {
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                }
                else {
                    if (ed::AcceptNewItem(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f)) {
                        if (finalSrc == ENTRY_NODE_ID) {
                            _currentGraph.entryStateHash = finalDst;
                        } else {
                            for (auto& s : _currentGraph.states) {
                                if (s.stateHash == finalSrc) {
                                    bool exists = false;
                                    for (auto& t : s.transitions) {
                                        if (t.targetStateHash == finalDst) { exists = true; break; }
                                    }
                                    if (!exists) {
                                        AnimTransition t;
                                        t.targetStateHash = finalDst;
                                        s.transitions.push_back(t);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        ed::PinId unusedPin;
        if (ed::QueryNewNode(&unusedPin)) ed::RejectNewItem();
    }
    ed::EndCreate();

    if (ed::BeginDelete()) {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId)) {
            if (ed::AcceptDeletedItem()) {
                int id = (int)(uintptr_t)linkId.Get();
                if (id == ENTRY_LINK_ID) {
                    _currentGraph.entryStateHash = 0;
                } else {
                    int currentId = 0x00000100;
                    bool deleted = false;
                    for (auto& s : _currentGraph.states) {
                        for (auto it = s.transitions.begin(); it != s.transitions.end(); ++it) {
                            if (currentId == id) {
                                s.transitions.erase(it);
                                deleted = true;
                                break;
                            }
                            currentId++;
                        }
                        if (deleted) break;
                    }
                }
            }
        }

        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId)) {
            uint32_t targetHash = (uint32_t)(uintptr_t)nodeId.Get();
            if (targetHash == ENTRY_NODE_ID) {
                ed::RejectDeletedItem();
                continue;
            }
            if (ed::AcceptDeletedItem()) {
                if (_currentGraph.entryStateHash == targetHash) _currentGraph.entryStateHash = 0;
                _currentGraph.states.erase(
                    std::remove_if(_currentGraph.states.begin(), _currentGraph.states.end(),
                        [targetHash](const AnimState& s) { return s.stateHash == targetHash; }),
                    _currentGraph.states.end());
            }
        }
    }
    ed::EndDelete();

    ed::Suspend();
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("CanvasContextMenu");
    }
    if (ImGui::BeginPopup("CanvasContextMenu")) {
        ImGui::Text("キャンバス操作 (Canvas Actions)");
        ImGui::Separator();
        if (ImGui::MenuItem("新しいステートを追加 (Add State)")) {
            std::string name = "NewState_" + std::to_string(_currentGraph.states.size());
            AnimState newState;
            newState.stateHash = CCL::Utils::HashString(name.c_str());
            newState.sequenceName = name;
            _currentGraph.states.push_back(newState);
            _nodeToPlaceHash = newState.stateHash;
        }
        ImGui::EndPopup();
    }
    ed::Resume();
}


void AnimGraphWindow::DrawInspector() {
    ImGui::Text("インスペクタ (Inspector)");
    ImGui::Separator();

    if (_selectedNodeHash != 0) {
        DrawStateSettings(_selectedNodeHash);
    }
    else if (_selectedLinkId != -1) {
        DrawTransitionSettings(_selectedLinkId);
    }
    else {
        ImGui::TextDisabled("項目を選択してください。\n(Select a Node or Link)");
    }
}

void AnimGraphWindow::DrawStateSettings(uint32_t stateHash) {
    if (stateHash == ENTRY_NODE_ID) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "開始地点 (Entry Node)");
        ImGui::Separator();
        ImGui::TextDisabled("ここから最初に再生したいステートへ\nワイヤーを引いて接続してください。");
        return;
    }

    for (auto& state : _currentGraph.states) {
        if (state.stateHash == stateHash) {
            char nameBuf[64];
            strcpy_s(nameBuf, state.sequenceName.c_str());
            if (ImGui::InputText("状態名 (State Name)", nameBuf, sizeof(nameBuf))) {
                state.sequenceName = nameBuf;
            }

            ImGui::Separator();

            // 2. 再生するアニメーション(Motion)の割り当てUI
            ImGui::Text("再生ファイル (Motion Sequence):");
            ImGui::TextDisabled("%s", state.sequenceFilePath.empty() ? "None" : state.sequenceFilePath.c_str());
            if (ImGui::Button("ファイルを選択 (Browse...)##Motion", ImVec2(-1, 0))) {
                char filename[MAX_PATH] = {};
                if (Dialog::OpenFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Select Anim Sequence", Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                    namespace fs = std::filesystem;
                    std::error_code ec;
                    fs::path relPath = fs::relative(filename, fs::current_path(), ec);
                    state.sequenceFilePath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;
                }
            }

            ImGui::Separator();

            ImGui::Checkbox("ループ再生 (Loop Animation)", &state.isLoop);
            ImGui::DragFloat("再生速度 (Playback Speed)", &state.playbackSpeed, 0.05f, 0.0f, 5.0f);

            char curveBuf[128];
            strcpy_s(curveBuf, state.speedCurveName.c_str());
            if (ImGui::InputText("速度カーブ (Speed Curve)", curveBuf, sizeof(curveBuf))) {
                state.speedCurveName = curveBuf;
            }

            ImGui::Separator();
            if (_currentGraph.entryStateHash == state.stateHash) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[ 初期状態 (Entry State) ]");
                ImGui::TextDisabled("※このノードが最初に再生されます。");
            }
            break;
        }
    }
}

void AnimGraphWindow::DrawTransitionSettings(int targetLinkId) {
    if (targetLinkId == ENTRY_LINK_ID) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "初期状態への遷移 (Entry Transition)");
        ImGui::Separator();
        ImGui::TextDisabled("※これはゲーム開始時に無条件で遷移するため、\n条件(Condition)を設定することはできません。");
        return;
    }

    int currentId = 0x00000100;
    for (auto& state : _currentGraph.states) {
        for (auto& trans : state.transitions) {
            if (currentId == targetLinkId) {

                std::string targetName = "Unknown";
                for (const auto& s : _currentGraph.states) {
                    if (s.stateHash == trans.targetStateHash) targetName = s.sequenceName;
                }

                ImGui::Text("遷移元 (From): %s", state.sequenceName.c_str());
                ImGui::Text("遷移先 (To): %s", targetName.c_str());
                ImGui::Separator();

                // ブレンド時間の調整UI
                ImGui::Text("遷移設定 (Transition Settings)");
                ImGui::DragFloat("ブレンド時間 (Blend Duration)", &trans.blendDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
                ImGui::Separator();

                ImGui::Text("遷移条件 (Conditions)");
                if (ImGui::Button("+ 条件を追加 (Add Condition)")) {
                    trans.conditions.push_back({
                        CCL::Utils::HashString("Speed"),
                        "Speed",
                        AnimConditionOp::Greater,
                        0.0f
                        });
                }

                for (size_t c = 0; c < trans.conditions.size(); ++c) {
                    auto& cond = trans.conditions[c];
                    ImGui::PushID((int)c);

                    ImGui::Separator();
                    ImGui::Text("条件 (Condition) %d", (int)c);

                    ImGui::Text("対象パラメータ (Parameter Name):");

                    char paramBuf[64];
                    strcpy_s(paramBuf, cond.paramName.c_str());
                    if (ImGui::InputText("##ParamName", paramBuf, sizeof(paramBuf))) {
                        cond.paramName = paramBuf;
                        cond.paramHash = CCL::Utils::HashString(paramBuf);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("パラメータ名を入力してください。ハッシュ値は自動計算されます。");
                    }
                    ImGui::TextDisabled("ID: 0x%08X", cond.paramHash);

  
                    int op = (int)cond.op;
                    ImGui::SetNextItemWidth(150);
                    // "再生終了 (Finished)" を選択肢に追加
                    const char* ops[] = { "一致 (Equal)", "より大きい (Greater)", "より小さい (Less)", "トリガー (Trigger)", "再生終了 (Finished)" };
                    if (ImGui::Combo("条件 (Op)", &op, ops, IM_ARRAYSIZE(ops))) {
                        cond.op = (AnimConditionOp)op;
                    }

                    // Trigger と Finished の場合は閾値(Value)を非表示にする
                    if (cond.op != AnimConditionOp::Trigger && cond.op != AnimConditionOp::Finished) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100);
                        ImGui::DragFloat("閾値 (Value)", &cond.threshold, 0.05f);
                    }
                    else {
                        cond.threshold = 0.0f;
                    }

                    if (ImGui::Button("削除 (Delete)")) {
                        trans.conditions.erase(trans.conditions.begin() + c);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::PopID();
                }
                return;
            }
            currentId++;
        }
    }
}

void AnimGraphWindow::DrawToolbar() {
    if (ImGui::Button("保存 (Save As...)")) {
        char filename[MAX_PATH] = {};
        auto result = Dialog::SaveFileName(
            filename, MAX_PATH, "JSON Files\0*.json\0", "Save Anim Graph", "json", Graphics::Instance().GetWindowHandle()
        );

        if (result == DialogResult::OK) {
            namespace fs = std::filesystem;
            fs::path absPath = filename;
            fs::path currentPath = fs::current_path();
            std::error_code ec;
            fs::path relPath = fs::relative(absPath, currentPath, ec);
            _currentFilePath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;
            SaveGraph(_currentFilePath);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("読込 (Load...)")) {
        char filename[MAX_PATH] = {};
        auto result = Dialog::OpenFileName(
            filename, MAX_PATH, "JSON Files\0*.json\0", "Select Anim Graph File", Graphics::Instance().GetWindowHandle()
        );

        if (result == DialogResult::OK) {
            namespace fs = std::filesystem;
            fs::path absPath = filename;
            fs::path currentPath = fs::current_path();
            std::error_code ec;
            fs::path relPath = fs::relative(absPath, currentPath, ec);
            _currentFilePath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;
            LoadGraph(_currentFilePath);
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("File: %s", _currentFilePath.empty() ? "None" : _currentFilePath.c_str());

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImGui::Button("全表示 (Focus All)")) ed::NavigateToContent();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(120);
    ImGui::InputText("##NewStateName", _newStateName, sizeof(_newStateName));
    ImGui::SameLine();
    if (ImGui::Button("追加 (+ Add State)") && _newStateName[0] != '\0') {
        AnimState newState;
        newState.stateHash = CCL::Utils::HashString(_newStateName);
        newState.sequenceName = _newStateName;
        _currentGraph.states.push_back(newState);
        _nodeToPlaceHash = newState.stateHash;
        _newStateName[0] = '\0';
    }
}

void AnimGraphWindow::SaveGraph(const std::string& path) {
    // ★ セーブ直前に、現在のノードエディタ上の座標を取得してデータに反映する
    if (_nodeContext) {
        ed::SetCurrentEditor(_nodeContext);

        ImVec2 entryPos = ed::GetNodePosition(ENTRY_NODE_ID);
        _currentGraph.entryPosX = entryPos.x;
        _currentGraph.entryPosY = entryPos.y;

        for (auto& state : _currentGraph.states) {
            ImVec2 pos = ed::GetNodePosition(state.stateHash);
            state.nodePosX = pos.x;
            state.nodePosY = pos.y;
        }
    }
    AnimGraphSerializer::SaveToJSON(_currentGraph, path);
}

void AnimGraphWindow::LoadGraph(const std::string& path) {
    if (AnimGraphSerializer::LoadFromJSON(_currentGraph, path, {})) {
        ed::ClearSelection();
        // ★ ロードが成功したら、次のフレームで座標を復元するフラグを立てる
        _needRestorePositions = true;
    }
}