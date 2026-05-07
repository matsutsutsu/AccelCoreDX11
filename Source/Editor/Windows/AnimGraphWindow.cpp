#include "AnimGraphWindow.h"
#include "Engine/GamePlay/Animation/Data/AnimGraphSerializer.h"
#include "Engine/Core/Math/StringHash.h"
#include <imgui_node_editor.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>

#include "Engine/Platform/Dialog.h"
#include "Engine/Platform/Logger.h"
//#include "Engine/Graphics/Core/Graphics.h"
#include <filesystem>
#include "ECS/Core/CCL_World.h"
#include <cmath> // sinf用
#include <unordered_map>

namespace ed = ax::NodeEditor;

const uint32_t ENTRY_NODE_ID = 0x0000000A; // 10
const uint32_t ENTRY_OUT_PIN = 0x0000000B; // 11
const int ENTRY_LINK_ID = 0x0000000C; // 12
const uint32_t ANY_STATE_NODE_ID = 0x0000000D; // 13
const uint32_t ANY_STATE_OUT_PIN = 0x0000000E; // 14
const int ANY_STATE_LINK_BASE_ID = 0x00000200; // 512

// =====================================================================
// ★追加: STEP 1 - 矢印を描画するための特製ヘルパー関数
// (AnimGraphWindow::DrawContents などの関数よりも「上」に書きます)
// =====================================================================
static void DrawLinkArrow(ImDrawList* drawList, const ImVec2& p0, const ImVec2& p3, ImU32 color) {
    if (p0.x == 0.0f && p0.y == 0.0f) return;
    if (p3.x == 0.0f && p3.y == 0.0f) return;

    // NodeEditorの標準的な水平リンク(左から右)のベジェ制御点計算
    float strength = std::abs(p3.x - p0.x) * 0.5f;
    strength = std::fmax(strength, 50.0f); // 最低でも50pxは曲げる

    ImVec2 p1 = ImVec2(p0.x + strength, p0.y);
    ImVec2 p2 = ImVec2(p3.x - strength, p3.y);

    // 曲線の中間地点 (t = 0.5) を計算
    float t = 0.5f, u = 0.5f; // t=0.5なのでuも0.5
    float w1 = u * u * u, w2 = 3 * u * u * t, w3 = 3 * u * t * t, w4 = t * t * t;

    ImVec2 midPoint = ImVec2(
        w1 * p0.x + w2 * p1.x + w3 * p2.x + w4 * p3.x,
        w1 * p0.y + w2 * p1.y + w3 * p2.y + w4 * p3.y
    );

    // 中間地点での接線ベクトル（曲線の傾き＝矢印の向き）を計算
    ImVec2 tangent = ImVec2(
        -3 * u * u * p0.x + (3 * u * u - 6 * u * t) * p1.x + (6 * u * t - 3 * t * t) * p2.x + 3 * t * t * p3.x,
        -3 * u * u * p0.y + (3 * u * u - 6 * u * t) * p1.y + (6 * u * t - 3 * t * t) * p2.y + 3 * t * t * p3.y
    );

    float length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
    if (length > 0.0001f) { tangent.x /= length; tangent.y /= length; }
    else { tangent = ImVec2(1, 0); }

    // 三角形の3頂点を計算
    float arrowSize = 10.0f; // 矢印の大きさ
    ImVec2 perp = ImVec2(-tangent.y, tangent.x); // 垂直ベクトル
    ImVec2 pt1 = ImVec2(midPoint.x + tangent.x * arrowSize, midPoint.y + tangent.y * arrowSize);
    ImVec2 pt2 = ImVec2(midPoint.x - tangent.x * arrowSize + perp.x * arrowSize * 0.7f, midPoint.y - tangent.y * arrowSize + perp.y * arrowSize * 0.7f);
    ImVec2 pt3 = ImVec2(midPoint.x - tangent.x * arrowSize - perp.x * arrowSize * 0.7f, midPoint.y - tangent.y * arrowSize - perp.y * arrowSize * 0.7f);

    drawList->AddTriangleFilled(pt1, pt2, pt3, color);
}

AnimGraphWindow::AnimGraphWindow() : EditorWindow("Anim Graph Editor") {
    _nodeContext = ed::CreateEditor();
}

AnimGraphWindow::~AnimGraphWindow() {
    if (_nodeContext) ed::DestroyEditor(_nodeContext);
}

void AnimGraphWindow::DrawContents(EditorContext& context) {
    ed::SetCurrentEditor(_nodeContext);

    // ================================================================
    // リアルタイム・デバッグ (選択中エンティティのステート同期)
    // ================================================================
    _runtimeActiveStateHash = 0;
    // エンティティが選択されており、ワールドが存在する場合
    if (context.selectedEntity != CCL::ECS::InvalidEntityID && context.world) {
        auto* fsm = context.world->GetComponent<AnimStateMachineComponent>(context.selectedEntity);
        if (fsm) {
            // 現在のステートハッシュを記憶する
            _runtimeActiveStateHash = fsm->currentStateHash;
        }
    }

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

    // =====================================================
    //  各ピンの中央座標を記憶するマップ
    // =====================================================
    std::unordered_map<uint32_t, ImVec2> pinPositions;

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

    // ★修正: 背景描画を BeginPin の外に出し、ピンの領域を文字だけに絞る
    ImVec2 cursorPosE = ImGui::GetCursorScreenPos();
    ImDrawList* dlE = ImGui::GetWindowDrawList();
    dlE->AddRectFilled(cursorPosE, ImVec2(cursorPosE.x + NODE_WIDTH, cursorPosE.y + 24), IM_COL32(204, 102, 25, 200), 4.0f);

    ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
    float twE = ImGui::CalcTextSize("● 線を伸ばす (Start)").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twE) * 0.5f);

    // ピンの宣言
    ed::BeginPin(ENTRY_OUT_PIN, ed::PinKind::Output);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "● 線を伸ばす (Start)");
    ed::EndPin();

    // ★追加: ピンの中央座標をマップに保存
    pinPositions[ENTRY_OUT_PIN] = ImVec2(
        ImGui::GetItemRectMin().x + ImGui::GetItemRectSize().x * 0.5f,
        ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y * 0.5f
    );

    ImGui::Dummy(ImVec2(NODE_WIDTH, 4));

    ed::EndNode();
    ed::PopStyleColor();

    // ロード直後なら、保存されていた座標に強制移動させる
    if (_needRestorePositions) {
        ed::SetNodePosition(ENTRY_NODE_ID, ImVec2(_currentGraph.entryPosX, _currentGraph.entryPosY));
    }

    // ================================================================
    // 1.5. ★追加: ANY STATE ノードの描画
    // ================================================================
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.1f, 0.3f, 0.4f, 1.0f)); // 青緑色
    ed::BeginNode(ANY_STATE_NODE_ID);

    ImGui::Dummy(ImVec2(NODE_WIDTH, 5));
    float anyW = ImGui::CalcTextSize("ANY STATE").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - anyW) * 0.5f);
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "ANY STATE");
    ImGui::Dummy(ImVec2(NODE_WIDTH, 5));

    ImGui::Separator();

    ImVec2 cursorPosAny = ImGui::GetCursorScreenPos();
    ImDrawList* dlAny = ImGui::GetWindowDrawList();
    dlAny->AddRectFilled(cursorPosAny, ImVec2(cursorPosAny.x + NODE_WIDTH, cursorPosAny.y + 24), IM_COL32(204, 102, 25, 200), 4.0f);

    ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
    float twAny = ImGui::CalcTextSize("● 線を伸ばす (Out)").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twAny) * 0.5f);

    ed::BeginPin(ANY_STATE_OUT_PIN, ed::PinKind::Output);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "● 線を伸ばす (Out)");
    ed::EndPin();

    pinPositions[ANY_STATE_OUT_PIN] = ImVec2(
        ImGui::GetItemRectMin().x + ImGui::GetItemRectSize().x * 0.5f,
        ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y * 0.5f
    );

    ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
    ed::EndNode();
    ed::PopStyleColor();

    if (_needRestorePositions) {
        ed::SetNodePosition(ANY_STATE_NODE_ID, ImVec2(_currentGraph.anyStatePosX, _currentGraph.anyStatePosY));
    }

    // ================================================================
    // 2. 通常ステートノードの描画
    // ================================================================
    for (auto& state : _currentGraph.states)
    {
        ImGui::PushID(state.stateHash);

        const bool isEntry = (_currentGraph.entryStateHash == state.stateHash);

        // ★このノードが現在実行中(アクティブ)かどうかの判定
        const bool isActive = (_runtimeActiveStateHash == state.stateHash);

        // --- スタイルの動的変更 ---
        ImVec4 nodeBgColor = isEntry ? ImVec4(0.15f, 0.3f, 0.15f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        if (isActive) {
            // アクティブ時は背景をサイバーな青緑に変更
            nodeBgColor = ImVec4(0.1f, 0.4f, 0.6f, 1.0f);
        }
        ed::PushStyleColor(ed::StyleColor_NodeBg, nodeBgColor);

        // アクティブなノードは枠線を太くしてパルス（点滅）発光させる
        if (isActive) {
            // 時間経過で 0.0 ~ 1.0 を往復するサイン波を作成
            float pulse = (sinf((float)ImGui::GetTime() * 8.0f) + 1.0f) * 0.5f;
            ImVec4 borderColor = ImVec4(0.2f + pulse * 0.4f, 0.7f + pulse * 0.3f, 1.0f, 1.0f);

            ed::PushStyleColor(ed::StyleColor_NodeBorder, borderColor);
            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 2.5f + pulse * 2.0f); // 脈打つ太さ
        }

        ed::BeginNode(state.stateHash);

        // --- 上部層: 遷移を受け入れるエリア (Input Pin) ---
        // ★修正: BeginPinの外で背景を描画し、ピンの判定領域を文字だけに絞る
        ImVec2 cursorPosI = ImGui::GetCursorScreenPos();
        ImDrawList* dlI = ImGui::GetWindowDrawList();
        dlI->AddRectFilled(cursorPosI, ImVec2(cursorPosI.x + NODE_WIDTH, cursorPosI.y + 24), IM_COL32(51, 153, 51, 200), 4.0f);

        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        float twI = ImGui::CalcTextSize("▼ ドロップで繋ぐ (In)").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twI) * 0.5f);

        // ここからピンの宣言
        ed::BeginPin(state.stateHash + 1, ed::PinKind::Input);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "▼ ドロップで繋ぐ (In)");
        ed::EndPin();

        // ★追加: ピンの中央座標をマップに保存 (文字の領域サイズで計算されるため正確になる)
        pinPositions[state.stateHash + 1] = ImVec2(
            ImGui::GetItemRectMin().x + ImGui::GetItemRectSize().x * 0.5f,
            ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y * 0.5f
        );

        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));

        ImGui::Separator();

        // --- 中央層: ノード移動エリア ---
        ImGui::Dummy(ImVec2(NODE_WIDTH, 8));

        // アクティブな場合は「▶」アイコンを付けて文字を緑色に光らせる
        std::string displayText = state.sequenceName;
        if (isActive) displayText = "▶ " + displayText;

        float tw = ImGui::CalcTextSize(displayText.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - tw) * 0.5f);

        if (isActive) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", displayText.c_str());
        }
        else {
            ImGui::TextUnformatted(displayText.c_str());
        }
        ImGui::Dummy(ImVec2(NODE_WIDTH, 8));

        ImGui::Separator();

        // --- 下部層: 線を伸ばすエリア (Output Pin) ---
        ImVec2 cursorPosO = ImGui::GetCursorScreenPos();
        ImDrawList* dlO = ImGui::GetWindowDrawList();
        dlO->AddRectFilled(cursorPosO, ImVec2(cursorPosO.x + NODE_WIDTH, cursorPosO.y + 24), IM_COL32(204, 102, 25, 200), 4.0f);

        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        float twO = ImGui::CalcTextSize("● 線を伸ばす (Out)").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (NODE_WIDTH - twO) * 0.5f);

        // ピンの宣言
        ed::BeginPin(state.stateHash + 2, ed::PinKind::Output);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "● 線を伸ばす (Out)");
        ed::EndPin();

        // ★追加: ピンの中央座標をマップに保存
        pinPositions[state.stateHash + 2] = ImVec2(
            ImGui::GetItemRectMin().x + ImGui::GetItemRectSize().x * 0.5f,
            ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y * 0.5f
        );

        ImGui::Dummy(ImVec2(NODE_WIDTH, 4));
        ed::EndNode();

        // 適用した発光スタイルを元に戻す
        if (isActive) {
            ed::PopStyleVar();   // NodeBorderWidth
            ed::PopStyleColor(); // NodeBorder
        }
        ed::PopStyleColor(); // NodeBg

        ImGui::PopID();

        // ================================================================
        // ★大復活: ロード直後の座標復元処理！ 
        // （これを忘れると全ノードが原点に密集して消えたように見えますわ）
        // ================================================================
        if (_needRestorePositions) {
            ed::SetNodePosition(state.stateHash, ImVec2(state.nodePosX, state.nodePosY));
        }
    }

    if (_nodeToPlaceHash != 0) {
        // ★修正: 画面中央ではなく、AnimState に設定した座標 (nodePosX, nodePosY) に配置する
        // (先ほどの HandleContextMenu で右クリックした CanvasPos が入っています)
        for (const auto& s : _currentGraph.states) {
            if (s.stateHash == _nodeToPlaceHash) {
                ed::SetNodePosition(_nodeToPlaceHash, ImVec2(s.nodePosX, s.nodePosY));
                break;
            }
        }

        ed::SelectNode(_nodeToPlaceHash);
        // ★生成直後に強制的にカメラが飛んでいく挙動をなくすため、NavigateToSelection は削除(またはコメントアウト)
        // ed::NavigateToSelection(false, 0.5f);
        _nodeToPlaceHash = 0;
    }
    // ================================================================
    // 3. ネイティブの線をしっかり描画する ＆ 矢印の追加
    // ================================================================
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (_currentGraph.entryStateHash != 0) {
        ed::Link(ENTRY_LINK_ID, ENTRY_OUT_PIN, _currentGraph.entryStateHash + 1, ImVec4(1.0f, 0.6f, 0.0f, 1.0f), 2.5f);
        
        // ★追加: Entryからの矢印描画
        if (pinPositions.count(ENTRY_OUT_PIN) && pinPositions.count(_currentGraph.entryStateHash + 1)) {
            DrawLinkArrow(drawList, pinPositions[ENTRY_OUT_PIN], pinPositions[_currentGraph.entryStateHash + 1], IM_COL32(255, 153, 0, 255));
        }
    }

    int linkId = 0x00000100;
    for (const auto& state : _currentGraph.states) {
        for (const auto& trans : state.transitions) {
            uint32_t srcPin = state.stateHash + 2;
            uint32_t dstPin = trans.targetStateHash + 1;

            ed::Link(linkId++, srcPin, dstPin, ImVec4(0.8f, 0.8f, 0.8f, 1.0f), 2.5f);

            // ★追加: 状態遷移の矢印描画
            if (pinPositions.count(srcPin) && pinPositions.count(dstPin)) {
                // アクティブ状態なら青く光らせる
                ImU32 arrowColor = (_runtimeActiveStateHash == state.stateHash) ? IM_COL32(100, 200, 255, 255) : IM_COL32(200, 200, 200, 255);
                DrawLinkArrow(drawList, pinPositions[srcPin], pinPositions[dstPin], arrowColor);
            }
        }
    }

    // Any State からのリンクを描画
    int anyLinkId = ANY_STATE_LINK_BASE_ID;
    for (const auto& trans : _currentGraph.anyStateTransitions) {
        uint32_t dstPin = trans.targetStateHash + 1;
        ed::Link(anyLinkId++, ANY_STATE_OUT_PIN, dstPin, ImVec4(0.4f, 0.8f, 0.8f, 1.0f), 2.5f);

        if (pinPositions.count(ANY_STATE_OUT_PIN) && pinPositions.count(dstPin)) {
            DrawLinkArrow(drawList, pinPositions[ANY_STATE_OUT_PIN], pinPositions[dstPin], IM_COL32(100, 200, 200, 255));
        }
    }

    ed::PopStyleVar();



    // すべてのノードの位置復元が終わったら、フラグを折って全体をカメラに収める
    if (_needRestorePositions) {
        ed::NavigateToContent();
        _needRestorePositions = false;
    }

    // ★ 注意: 古い「4. 矢印を安全にオーバーレイ描画」のブロック (ed::Suspend(); 〜 ed::Resume();) はここで完全に削除します。

    HandleInteraction();

    // =========================================================
    // ★ここにあった古い Suspend() 〜 Resume() のコンテキストメニュー処理を削除し、
    // 新しく作った HandleContextMenu() に置き換えます。
    // =========================================================
    HandleContextMenu();

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
    // -------------------------------------------------------------
    // ★ 革命: ピンの所属と種類を確実に特定する万能ヘルパー
    // -------------------------------------------------------------
    auto getPinInfo = [&](uint32_t pinId, bool& isInput, uint32_t& nodeHash) -> bool {
        if (pinId == ENTRY_OUT_PIN) { isInput = false; nodeHash = ENTRY_NODE_ID; return true; }
        if (pinId == ANY_STATE_OUT_PIN) { isInput = false; nodeHash = ANY_STATE_NODE_ID; return true; }
        for (auto& s : _currentGraph.states) {
            if (pinId == s.stateHash + 1) { isInput = true; nodeHash = s.stateHash; return true; }
            if (pinId == s.stateHash + 2) { isInput = false; nodeHash = s.stateHash; return true; }
        }
        return false; // 存在しないピン
        };

    // =============================================================
    // 1. ノードを繋ぐ（Create Link）の安全な処理
    // =============================================================
    if (ed::BeginCreate()) {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {
            if (startPinId && endPinId) {
                uint32_t startId = (uint32_t)(uintptr_t)startPinId.Get();
                uint32_t endId = (uint32_t)(uintptr_t)endPinId.Get();

                bool isStartInput = false, isEndInput = false;
                uint32_t srcHash = 0, dstHash = 0;

                // 両方のピンの情報を完全に特定できた場合のみ進行
                if (getPinInfo(startId, isStartInput, srcHash) && getPinInfo(endId, isEndInput, dstHash)) {

                    // 入力同士、または出力同士の接続は弾く
                    if (isStartInput == isEndInput) {
                        ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                    }
                    else {
                        // 出発地(Src)と目的地(Dst)を確定させる（逆引きドラッグ対策）
                        uint32_t finalSrc = isStartInput ? dstHash : srcHash;
                        uint32_t finalDst = isStartInput ? srcHash : dstHash;

                        // Any State や Entry は「非常口」なので、そこに向かう入力(目的地)には絶対になれない
                        if (finalDst == ENTRY_NODE_ID || finalDst == ANY_STATE_NODE_ID) {
                            ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                        }
                        else if (ed::AcceptNewItem(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f)) {

                            if (finalSrc == ENTRY_NODE_ID) {
                                _currentGraph.entryStateHash = finalDst;
                            }
                            else if (finalSrc == ANY_STATE_NODE_ID) {
                                // Any State 遷移の登録
                                bool exists = false;
                                for (auto& t : _currentGraph.anyStateTransitions) {
                                    if (t.targetStateHash == finalDst) { exists = true; break; }
                                }
                                if (!exists) {
                                    AnimTransition t;
                                    t.targetStateHash = finalDst;
                                    _currentGraph.anyStateTransitions.push_back(t);
                                }
                            }
                            else {
                                // 通常ステート遷移の登録
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
                else {
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                }
            }
        }
        ed::PinId unusedPin;
        if (ed::QueryNewNode(&unusedPin)) ed::RejectNewItem();
    }
    ed::EndCreate();

    // =============================================================
    // 2. ノードや線を消す（Delete Item）の大掃除処理
    // =============================================================
    if (ed::BeginDelete()) {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId)) {
            if (ed::AcceptDeletedItem()) {
                int id = (int)(uintptr_t)linkId.Get();
                if (id == ENTRY_LINK_ID) {
                    _currentGraph.entryStateHash = 0;
                }
                else if (id >= ANY_STATE_LINK_BASE_ID && id < ANY_STATE_LINK_BASE_ID + 1000) {
                    int currentAnyId = ANY_STATE_LINK_BASE_ID;
                    for (auto it = _currentGraph.anyStateTransitions.begin(); it != _currentGraph.anyStateTransitions.end(); ++it) {
                        if (currentAnyId == id) {
                            _currentGraph.anyStateTransitions.erase(it);
                            break;
                        }
                        currentAnyId++;
                    }
                }
                else {
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
            // Any State や Entry ノードは不変の基盤なので削除不可
            if (targetHash == ENTRY_NODE_ID || targetHash == ANY_STATE_NODE_ID) {
                ed::RejectDeletedItem();
                continue;
            }

            if (ed::AcceptDeletedItem()) {
                if (_currentGraph.entryStateHash == targetHash) _currentGraph.entryStateHash = 0;

                // ★大掃除1: Any Stateから、今削除されたノードへ向かっている矢印を切断する
                _currentGraph.anyStateTransitions.erase(
                    std::remove_if(_currentGraph.anyStateTransitions.begin(), _currentGraph.anyStateTransitions.end(),
                        [targetHash](const AnimTransition& t) { return t.targetStateHash == targetHash; }),
                    _currentGraph.anyStateTransitions.end());

                // ★大掃除2: 他の全ステートからも、今削除されたノードへの矢印を切断する
                for (auto& s : _currentGraph.states) {
                    s.transitions.erase(
                        std::remove_if(s.transitions.begin(), s.transitions.end(),
                            [targetHash](const AnimTransition& t) { return t.targetStateHash == targetHash; }),
                        s.transitions.end());
                }

                // 本体（ステート）の削除
                _currentGraph.states.erase(
                    std::remove_if(_currentGraph.states.begin(), _currentGraph.states.end(),
                        [targetHash](const AnimState& s) { return s.stateHash == targetHash; }),
                    _currentGraph.states.end());
            }
        }
    }
    ed::EndDelete();
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
                if (Dialog::OpenFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Select Anim Sequence",
                    "Data/Animations/Node", GetActiveWindow()) == DialogResult::OK) {
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

            // ルートモーションカーブ
            char rmCurveBuf[256];
            strncpy_s(rmCurveBuf, state.rootMotionCurveName.c_str(), sizeof(rmCurveBuf));
            if (ImGui::InputText("ルートモーションカーブ (RootMotion Curve)", rmCurveBuf, sizeof(rmCurveBuf))) {
                state.rootMotionCurveName = rmCurveBuf;
            }

            ImGui::Separator();
            if (_currentGraph.entryStateHash == state.stateHash) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[ 初期状態 (Entry State) ]");
                ImGui::TextDisabled("※このノードが最初に再生されます。");
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "◆ 動的カーブ設定 (Curves)");



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

    //  Any State 遷移の設定インスペクタ
    if (targetLinkId >= ANY_STATE_LINK_BASE_ID && targetLinkId < ANY_STATE_LINK_BASE_ID + 1000) {
        int currentAnyId = ANY_STATE_LINK_BASE_ID;
        for (auto& trans : _currentGraph.anyStateTransitions) {
            if (currentAnyId == targetLinkId) {
                std::string targetName = "Unknown";
                for (const auto& s : _currentGraph.states) {
                    if (s.stateHash == trans.targetStateHash) targetName = s.sequenceName;
                }

                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "特別遷移 (Any State Transition)");
                ImGui::Text("遷移先 (To): %s", targetName.c_str());
                ImGui::Separator();

                DrawTransitionConditionUI(trans);
                return;
            }
            currentAnyId++;
        }
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

                // =======================================================
                //  AND / OR の切り替えUI
                // =======================================================
                int logicOp = (int)trans.logicType;
                const char* logicOps[] = { "すべて満たす (AND)", "いずれかを満たす (OR)" };
                if (ImGui::Combo("判定方法 (Logic)", &logicOp, logicOps, IM_ARRAYSIZE(logicOps))) {
                    trans.logicType = (AnimTransitionLogic)logicOp;
                }
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
            filename, MAX_PATH, "JSON Files\0*.json\0", "Save Anim Graph", "json",
            "Data/Animations/Node", GetActiveWindow()
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
            filename, MAX_PATH, "JSON Files\0*.json\0", "Select Anim Graph File",
            "Data/Animations/Node", GetActiveWindow()
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

        //  Any Stateの座標も保存
        ImVec2 anyPos = ed::GetNodePosition(ANY_STATE_NODE_ID);
        _currentGraph.anyStatePosX = anyPos.x;
        _currentGraph.anyStatePosY = anyPos.y;
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

void AnimGraphWindow::HandleContextMenu() {
    ed::Suspend();

    // キャンバスの背景で右クリックされたか判定
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("AnimCanvasContextMenu");
        // ★右クリックした瞬間のマウス座標（スクリーン座標）を記憶
        _newNodeSpawnPos = ImGui::GetMousePosOnOpeningCurrentPopup();
    }

    if (ImGui::BeginPopup("AnimCanvasContextMenu")) {
        ImGui::Text("キャンバス操作 (Canvas Actions)");
        ImGui::Separator();

        // 状態名を入力させる
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##NewStateNameCtx", _newStateName, sizeof(_newStateName));
        ImGui::SameLine();

        // 追加ボタン
        if (ImGui::Button("新しいステートを追加") && _newStateName[0] != '\0') {

            AnimState newState;
            newState.stateHash = CCL::Utils::HashString(_newStateName);
            newState.sequenceName = _newStateName;

            // ★記憶しておいたスクリーン座標を、広大なキャンバスの仮想座標（Canvas）に変換する！
            ImVec2 canvasPos = ed::ScreenToCanvas(_newNodeSpawnPos);

            // ノードの初期座標を設定（Bake時などに使われる構造体の値）
            newState.nodePosX = canvasPos.x;
            newState.nodePosY = canvasPos.y;

            _currentGraph.states.push_back(newState);

            // ★生成した瞬間にその場所にノードを配置するように指示
            _nodeToPlaceHash = newState.stateHash;

            // 入力欄をクリア
            _newStateName[0] = '\0';

            // メニューを閉じる
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ed::Resume();
}

// ★追加: インスペクタのUIを共通化する関数
void AnimGraphWindow::DrawTransitionConditionUI(AnimTransition& trans) {
    ImGui::Text("遷移設定 (Transition Settings)");
    ImGui::DragFloat("ブレンド時間 (Blend Duration)", &trans.blendDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
    ImGui::Separator();

    int logicOp = (int)trans.logicType;
    const char* logicOps[] = { "すべて満たす (AND)", "いずれかを満たす (OR)" };
    if (ImGui::Combo("判定方法 (Logic)", &logicOp, logicOps, IM_ARRAYSIZE(logicOps))) {
        trans.logicType = (AnimTransitionLogic)logicOp;
    }
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
        const char* ops[] = { "一致 (Equal)", "より大きい (Greater)", "より小さい (Less)", "トリガー (Trigger)", "再生終了 (Finished)" };
        if (ImGui::Combo("条件 (Op)", &op, ops, IM_ARRAYSIZE(ops))) {
            cond.op = (AnimConditionOp)op;
        }

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
}