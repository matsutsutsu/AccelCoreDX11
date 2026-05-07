/**
 * @file BehaviorTreeWindow.cpp
 * @brief BehaviorTree エディタの実装
 */
#include "BehaviorTreeWindow.h"
#include "Editor/Core/EditorCommandHistory.h"
#include "Game/Logics/AI/BehaviorTree/Data/ActionRegistry.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeComponents.h"
#include <imgui_node_editor.h>
#include <imgui.h>
#include <algorithm>
#include <queue>
#include <fstream>
#include <json.hpp>
#include "Engine/Platform/Logger.h"
#include "Engine/Platform/Dialog.h"


using json = nlohmann::json;

 // 長い名前空間を "ed" に短縮するお作法
namespace ed = ax::NodeEditor;


// グラフの構造変化を記録する専用コマンド
class BTGraphChangeCommand : public IEditorCommand {
    BehaviorTreeWindow* _window;
    nlohmann::json _oldState;
    nlohmann::json _newState;
    std::string _name;
public:
    BTGraphChangeCommand(BehaviorTreeWindow* window, const std::string& name, const nlohmann::json& oldState, const nlohmann::json& newState)
        : _window(window), _name(name), _oldState(oldState), _newState(newState) {
    }

    void Execute() override {
        _window->RestoreGraphState(_newState);
        CCL_LOG_INFO(LogCategory::Editor, "[BT Editor] Redo: %s", _name.c_str());
    }
    void Undo() override {
        _window->RestoreGraphState(_oldState);
        CCL_LOG_INFO(LogCategory::Editor, "[BT Editor] Undo: %s", _name.c_str());
    }
};

void BehaviorTreeWindow::RecordStateForUndo(const std::string& commandName) {
    nlohmann::json newState = GetGraphStateAsJson();
    // 状態が変わっていればコマンドをスタックに積む
    if (_lastSavedState != newState) {
        EditorCommandHistory::Instance().ExecuteCommand(
            std::make_unique<BTGraphChangeCommand>(this, commandName, _lastSavedState, newState)
        );
        _lastSavedState = newState;
    }
}


BehaviorTreeWindow::BehaviorTreeWindow() : EditorWindow("Behavior Tree Editor") {
    _nodeContext = ed::CreateEditor();

    // 初期状態としてRootノードを1つだけ作成しておく
    CreateNode(EditorBTNodeType::Root, "ROOT", 0.0f, 0.0f);
}

BehaviorTreeWindow::~BehaviorTreeWindow() {
    if (_nodeContext) ed::DestroyEditor(_nodeContext);
}

// ====================================================================
// Undo / Redo 用：現在のグラフ状態をJSONとして抽出する
// ====================================================================
nlohmann::json BehaviorTreeWindow::GetGraphStateAsJson() {
    nlohmann::json root;
    if (_nodeContext) {
        ed::SetCurrentEditor(_nodeContext);
    }

    nlohmann::json jNodes = nlohmann::json::array();
    for (auto& node : _nodes) {
        nlohmann::json jn;
        jn["id"] = node.id;
        jn["type"] = static_cast<int>(node.type);
        jn["name"] = node.name;
        jn["actionId"] = static_cast<int>(node.actionOrConditionId);

        // ピンのID
        jn["inputPinId"] = node.inputPin.id;
        jn["outputPinId"] = node.outputPin.id;

        // 座標情報の取得
        if (node.positionInitialized) {
            ImVec2 pos = ed::GetNodePosition(node.id);
            jn["posX"] = pos.x;
            jn["posY"] = pos.y;
        }
        else {
            jn["posX"] = node.posX;
            jn["posY"] = node.posY;
        }

        // グループ固有のデータ
        if (node.type == EditorBTNodeType::Group) {
            jn["width"] = node.width;
            jn["height"] = node.height;
        }

        // デコレーター固有のデータ
        if (node.type == EditorBTNodeType::Decorator) {
            jn["decoratorType"] = node.decoratorType;
            jn["decoratorParam"] = node.decoratorParam;
        }

        jNodes.push_back(jn);
    }
    root["editor_nodes"] = jNodes;

    nlohmann::json jLinks = nlohmann::json::array();
    for (auto& link : _links) {
        nlohmann::json jl;
        jl["id"] = link.id;
        jl["startPin"] = link.startPinId;
        jl["endPin"] = link.endPinId;
        jLinks.push_back(jl);
    }
    root["editor_links"] = jLinks;
    root["last_id_counter"] = _idCounter;

    return root;
}

// ====================================================================
// Undo / Redo 用：渡されたJSONからグラフ状態を強制復元する
// ====================================================================
void BehaviorTreeWindow::RestoreGraphState(const nlohmann::json& state) {
    if (!state.contains("editor_nodes")) return;

    // =========================================================
    // 破棄する前に、現在の選択状態を記憶しておく
    // =========================================================
    std::vector<ed::NodeId> selectedNodes;
    if (_nodeContext) {
        ed::SetCurrentEditor(_nodeContext);
        selectedNodes.resize(ed::GetSelectedObjectCount());
        int count = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
        selectedNodes.resize(count);
    }

    // 現在の状態を破棄
    _nodes.clear();
    _links.clear();

    if (_nodeContext) {
        ed::SetCurrentEditor(_nodeContext);
        ed::ClearSelection(); // ゴースト選択状態によるクラッシュを防ぐ
    }

    _idCounter = state.value("last_id_counter", 1);

    // ノードの復元
    for (auto& jn : state["editor_nodes"]) {
        EditorBTNode node;
        node.id = jn["id"];
        node.type = static_cast<EditorBTNodeType>(jn["type"].get<int>());
        node.name = jn.value("name", "Unknown");
        node.actionOrConditionId = static_cast<ActionID>(jn.value("actionId", 0));
        node.posX = jn.value("posX", 0.0f);
        node.posY = jn.value("posY", 0.0f);

        if (node.type == EditorBTNodeType::Group) {
            node.width = jn.value("width", 400.0f);
            node.height = jn.value("height", 400.0f);
        }
        if (node.type == EditorBTNodeType::Decorator) {
            node.decoratorType = jn.value("decoratorType", 0);
            node.decoratorParam = jn.value("decoratorParam", 0.0f);
        }

        node.inputPin = { jn.value("inputPinId", 0u), node.id, true };
        node.outputPin = { jn.value("outputPinId", 0u), node.id, false };

        // 座標をキャンバスに再適用させるため false にしておく
        node.positionInitialized = false;

        _nodes.push_back(node);
    }

    // リンクの復元
    if (state.contains("editor_links")) {
        for (auto& jl : state["editor_links"]) {
            BTLink link;
            link.id = jl["id"];
            link.startPinId = jl["startPin"];
            link.endPinId = jl["endPin"];
            _links.push_back(link);
        }
    }

    // =========================================================
    //  復元が終わったら、記憶していた選択状態を元に戻す
    // =========================================================
    if (_nodeContext && !selectedNodes.empty()) {
        for (auto id : selectedNodes) {
            ed::SelectNode(id, true); // true = 複数選択のアペンド
        }
    }

    _executionOrderDirty = true;
}
void BehaviorTreeWindow::CreateNode(EditorBTNodeType type, const std::string& name, float x, float y) {
    EditorBTNode node;
    node.id = GetNextId();
    node.type = type;
    node.name = name;

    // グループとルート以外は入力ピンを持つ
    if (type != EditorBTNodeType::Root && type != EditorBTNodeType::Group) {
        node.inputPin = { GetNextId(), node.id, true };
    }
    else {
        node.inputPin.id = 0;
    }

    // アクション/条件/グループ 以外は出力ピンを持つ
    if (type != EditorBTNodeType::Condition && type != EditorBTNodeType::Action && type != EditorBTNodeType::Group) {
        node.outputPin = { GetNextId(), node.id, false };
    }
    else {
        node.outputPin.id = 0;
    }

    node.posX = x;
    node.posY = y;
    node.positionInitialized = false;
    _nodes.push_back(node);
}

void BehaviorTreeWindow::DrawContents(EditorContext& context) {
    ed::SetCurrentEditor(_nodeContext);

    // カラムを分けて、左側をノードエディタ、右側をインスペクタにする（AnimGraphと同様）
    ImGui::Columns(2, "BTEditorColumns", true);
    // =======================================================
    // ★ 修正: 初回起動時のみ、ノードエディタの幅を75%にする
    // =======================================================
    static bool isFirstLayout = true;
    if (isFirstLayout) {
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.75f);
        isFirstLayout = false;
    }

    DrawToolbar();
    DrawNodeWorkspace(context);

    ImGui::NextColumn();
    DrawInspector();
    ImGui::Columns(1);
}

// ====================================================================
// 描画：ツールバー（「日本語（英語名）」ボタン）
// ====================================================================
void BehaviorTreeWindow::DrawToolbar() {
    if (ImGui::Button("保存 ( Save )")) {
        char filename[256] = {};

        // プリセット DialogPreset::Model を直接渡すだけ
        if (Dialog::OpenFileName(filename, sizeof(filename), DialogPreset::BehaviorTree, GetActiveWindow()) == DialogResult::OK) {
            _currentFilePath = filename;
            SaveGraph(_currentFilePath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("開く (Open...)")) {
        char filename[256] = {};

        // プリセット DialogPreset::Model を直接渡すだけ
        if (Dialog::OpenFileName(filename, sizeof(filename), DialogPreset::BehaviorTree, GetActiveWindow()) == DialogResult::OK) {
            _currentFilePath = filename;
            LoadGraph(_currentFilePath);
        }
    }

    ImGui::Separator();
    
    // ノード追加ボタン
    //if (ImGui::Button("+ セレクター (Selector)")) 
    //    CreateNode(EditorBTNodeType::Selector, "セレクター (Selector)", 200, 0); 
    //ImGui::SameLine();
    //if (ImGui::Button("+ シーケンス (Sequence)")) 
    //    CreateNode(EditorBTNodeType::Sequence, "シーケンス (Sequence)", 200, 50); 
    //ImGui::SameLine();
    //if (ImGui::Button("+ コンディション (Condition)")) 
    //    CreateNode(EditorBTNodeType::Condition, "コンディション (Condition)", 400, 0); 
    //ImGui::SameLine();
    //if (ImGui::Button("+ アクション (Action)")) 
    //    CreateNode(EditorBTNodeType::Action, "アクション (Action)", 400, 50);
    //ImGui::SameLine();
    //if (ImGui::Button("+ デコレーター (Decorator)"))
    //    CreateNode(EditorBTNodeType::Decorator, "デコレーター (Decorator)", 600, 0);

    //ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    ImGui::Checkbox("自動追従 (Auto Follow)", &_autoFollow);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("チェックを入れると、ゲーム実行中に現在アクティブなノードへカメラが自動で移動します。");
    }
}

/**
 * @brief 毎フレーム、親子関係から実行優先順位を計算する
 * @note DOD（データ指向設計）において、配列の並び順はキャッシュ効率に直結する重要な情報である
 */
void BehaviorTreeWindow::UpdateExecutionOrder() {
    // 全ての順番を一旦リセット
    for (auto& n : _nodes) n.executionOrder = -1;

    // 1. 各親ノードに対して、接続されている子を取得
    for (auto& parent : _nodes) {
        if (parent.outputPin.id == 0) continue;

        std::vector<EditorBTNode*> children;
        for (const auto& link : _links) {
            if (link.startPinId == parent.outputPin.id) {
                for (auto& child : _nodes) {
                    if (child.inputPin.id == link.endPinId) {
                        children.push_back(&child);
                    }
                }
            }
        }

        // ★ 修正：posY（Y座標）の比較から、posX（X座標）の比較に変更するだけ！
        std::sort(children.begin(), children.end(), [](EditorBTNode* a, EditorBTNode* b) {
            return a->posX < b->posX;
            });

        // 3. 順序番号を割り当てる（1から開始）
        for (int i = 0; i < (int)children.size(); ++i) {
            children[i]->executionOrder = i + 1;
        }
    }
}

/**
 * @brief ノードエディタ空間（キャンバス）の描画と、リンク操作の監視を行う
 */
void BehaviorTreeWindow::DrawNodeWorkspace(EditorContext& context) {
    // リンクやノードに変更があった時だけソート処理を走らせる
    if (_executionOrderDirty) {
        UpdateExecutionOrder();
        _executionOrderDirty = false;
    }

    ed::Begin("BT_Node_Workspace");

    // =======================================================
    //  選択中のエンティティから、実行中のAI状態（デバッグ配列）を取得
    // =======================================================
    BehaviorTreeComponent* activeAI = nullptr;
    if (context.selectedEntity != CCL::ECS::InvalidEntityID && context.world) {
        activeAI = context.world->GetComponent<BehaviorTreeComponent>(context.selectedEntity);
    }

    uint32_t currentActiveNodeId = 0;
    bool isActionNodeRunning = false;

    for (auto& node : _nodes) {
        if (!node.positionInitialized) {
            ed::SetNodePosition(node.id, ImVec2(node.posX, node.posY));
            node.positionInitialized = true;
        }

        // =========================================================
        // グループ（コメント）ノードの専用描画 (安定版ベースのUE5装飾)
        // =========================================================
        if (node.type == EditorBTNodeType::Group) {

            // --- A. 選択状態による動的なハイライト（UE5風オレンジ発光） ---
            bool isSelected = ed::IsNodeSelected(node.id);
            ImColor borderColor = isSelected ? ImColor(255, 165, 0, 255) : ImColor(150, 150, 150, 80);
            ImColor bgColor = isSelected ? ImColor(40, 40, 40, 100) : ImColor(30, 30, 30, 60);

            ed::PushStyleColor(ed::StyleColor_GroupBg, bgColor);
            ed::PushStyleColor(ed::StyleColor_GroupBorder, borderColor);
            ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);

            // ★ BeginNode を先に呼ぶ
            ed::BeginNode(node.id);
            ImGui::PushID(node.id);

            // --- LOD: ズームに応じてフォントサイズを調整してラベルを描画 ---
            float currentZoom = ed::GetCurrentZoom();
            ImVec2 labelScreenPos = ImGui::GetCursorScreenPos();

            // ===============================================================
            // ★ 無限膨張バグを防ぐため、あなたの正解コードである 1.0f x 1.0f を使用！
            // ===============================================================
            ImGui::Dummy(ImVec2(1.0f, 1.0f));

            ImDrawList* groupDrawList = ImGui::GetWindowDrawList();

            // --- B. 背景グラフィックとしてのヘッダー帯の描画 ---
            // ※ ダミーで領域を取るのではなく、文字の後ろに直接「絵」として描くことで膨張を防ぐ
            float headerHeight = 28.0f;
            ImVec2 headerMax = ImVec2(labelScreenPos.x + node.width, labelScreenPos.y + headerHeight);

            // ヘッダーの背景色と下部のライン
            groupDrawList->AddRectFilled(labelScreenPos, headerMax, ImColor(50, 50, 50, 180), 8.0f, ImDrawFlags_RoundCornersTop);
            groupDrawList->AddLine(ImVec2(labelScreenPos.x, headerMax.y), headerMax, ImColor(0, 0, 0, 150), 2.0f);

            if (currentZoom < 0.6f) {
                // ズームアウト時はフォントを拡大して視認性を保つ
                float customFontSize = ImGui::GetFontSize() / currentZoom * 0.8f;
                groupDrawList->AddText(
                    ImGui::GetFont(), customFontSize,
                    labelScreenPos, ImColor(220, 220, 220, 255),
                    node.name.c_str());
            }
            else {
                // 通常時のテキスト（少し右下にズラして綺麗に配置）
                ImVec2 textPos = ImVec2(labelScreenPos.x + 8, labelScreenPos.y + 6);
                groupDrawList->AddText(textPos, ImColor(220, 220, 220, 255), ("// " + node.name).c_str());
            }

            // ★ ed::Group() がリサイズハンドルを自動描画・管理する
            ed::Group(ImVec2(node.width, node.height));

            ImGui::PopID();
            ed::EndNode();

            ed::PopStyleVar(1);
            ed::PopStyleColor(2);

            // ★ EndNode 後にエディタが管理している実際のサイズを読み返す
            ImVec2 actualSize = ed::GetNodeSize(node.id);
            if (actualSize.x > 10.0f && actualSize.y > 10.0f) {
                // 最小サイズを下回らないようにガード
                node.width = (std::max)(100.0f, actualSize.x);
                node.height = (std::max)(50.0f, actualSize.y);
            }

            // 座標を更新して次のノードへ
            ImVec2 p = ed::GetNodePosition(node.id);
            node.posX = p.x;
            node.posY = p.y;
            continue;
        }

        // =======================================================
        // 1. スタイリング：ノードの種類に応じた色の決定（UE5準拠）
        // =======================================================
        ImColor headerColor;
        switch (node.type) {
        case EditorBTNodeType::Root:      headerColor = ImColor(40, 40, 40); break;      // 黒
        case EditorBTNodeType::Selector:  headerColor = ImColor(120, 30, 30); break;    // 深い赤
        case EditorBTNodeType::Sequence:  headerColor = ImColor(30, 80, 120); break;    // 深い青
        case EditorBTNodeType::Decorator: headerColor = ImColor(120, 100, 20); break;   // 金/黄
        case EditorBTNodeType::Condition: headerColor = ImColor(30, 120, 60); break;    // 緑
        case EditorBTNodeType::Action:    headerColor = ImColor(40, 100, 150); break;   // 明るい青
        default:                          headerColor = ImColor(100, 100, 100); break;
        }

        // =======================================================
        //  リアルタイムデバッグ表示（状態による枠線の発光）
        // =======================================================
        ImColor borderColor = ImColor(60, 60, 60, 255); // 通常の枠線色
        float borderWidth = 1.0f;

        // activeAIが存在し、エディタノードがランタイム配列と紐付いている場合のみ状態を見る
        if (activeAI && node.runtimeFlatIndex >= 0 && node.runtimeFlatIndex < activeAI->debugNodeStates.size()) {
            BTDebugState state = activeAI->debugNodeStates[node.runtimeFlatIndex];

            if (state == BTDebugState::Running) {
                // 実行中（Running）は黄色く脈打つ
                float blink = (sinf(ImGui::GetTime() * 15.0f) * 0.5f) + 0.5f;
                borderColor = ImColor(255, 200, 0, 200 + static_cast<int>(55 * blink));
                borderWidth = 3.0f;

                // 親ノード(Sequence等)もRunningになるが、実際に作業している末端の「Action」ノードを優先してカメラで追う
                if (!isActionNodeRunning || node.type == EditorBTNodeType::Action) {
                    currentActiveNodeId = node.id;
                    if (node.type == EditorBTNodeType::Action) {
                        isActionNodeRunning = true;
                    }
                }

            }
            else if (state == BTDebugState::Success) {
                // 成功（Success）は緑色
                borderColor = ImColor(50, 255, 50, 200);
                borderWidth = 2.0f;
            }
            else if (state == BTDebugState::Failure) {
                // 失敗（Failure）は赤色
                borderColor = ImColor(255, 50, 50, 200);
                borderWidth = 2.0f;
            }
        }

        // ノード全体の背景とボーダーを設定
        ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(25, 25, 25, 240));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, borderColor);
        ed::PushStyleVar(ed::StyleVar_NodeRounding, 6.0f);
        ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, borderWidth); // 枠線の太さを変更可能に

        // =========================================================
        // 2. ノード描画開始
        // =========================================================
        ed::BeginNode(node.id);

        const float NODE_WIDTH = 180.0f; // 幅を少し広くしてプロ感を出す
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // --- A. 上部入力バー（UE5スタイル） ---
        if (node.inputPin.id != 0) {
            ed::BeginPin(node.inputPin.id, ed::PinKind::Input);
            ImGui::Dummy(ImVec2(NODE_WIDTH, 20.0f)); // ★高さを12→20に拡大し、掴みやすくした

            ImVec2 pMin = ImGui::GetItemRectMin();
            ImVec2 pMax = ImGui::GetItemRectMax();
            // 接続バーの描画
            drawList->AddRectFilled(ImVec2(pMin.x + 2, pMin.y + 14), ImVec2(pMax.x - 2, pMax.y - 2), ImColor(180, 180, 180), 2.0f);
            ed::EndPin();
        }

        // --- B. ヘッダー（タイトル部分） ---
        ImGui::BeginGroup();
        // ヘッダー背景の描画
        ImVec2 headerStart = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(NODE_WIDTH, 32.0f));
        ImVec2 headerEnd = ImGui::GetItemRectMax();
        drawList->AddRectFilled(headerStart, headerEnd, headerColor, 4.0f, ImDrawFlags_RoundCornersTop);

        // タイトル文字の配置
        ImGui::SetCursorScreenPos(ImVec2(headerStart.x + 10, headerStart.y + 8));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // 必要なら太字フォント等に変更
        ImGui::Text("%s", node.name.c_str());
        ImGui::PopFont();
        ImGui::EndGroup();

        // --- C. コンテンツ（詳細情報） ---
        ImGui::BeginGroup();
        ImGui::Indent(10.0f);
        ImGui::Spacing();

        if (node.executionOrder != -1 && node.type != EditorBTNodeType::Group) {
            ImGui::TextColored(ImVec4(1, 1, 0, 0.8f), "Priority: %d", node.executionOrder);
        }

        if (node.type == EditorBTNodeType::Action) {
            const char* actionName = "Unknown";
            for (int i = 0; i < g_ActionRegistryCount; ++i) {
                if (g_ActionRegistry[i].id == node.actionOrConditionId) {
                    actionName = g_ActionRegistry[i].name; break;
                }
            }
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "> %s", actionName);

            // =======================================================
              // ★ 修正：Waitアクション時の残り時間リアルタイム表示
              // =======================================================
            if (activeAI && node.runtimeFlatIndex >= 0 && node.runtimeFlatIndex < activeAI->nodeTimers.size()) {
                float currentTimer = activeAI->nodeTimers[node.runtimeFlatIndex];

                // ★ 究極の修正：
                // タイマーが0.0fより大きい【かつ】、そのノードのIDが本物のWaitノード(100〜111)である時だけ表示する！
                bool isRealWaitNode = (node.actionOrConditionId >= 100 && node.actionOrConditionId <= 111);

                if (currentTimer > 0.0f && isRealWaitNode) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Wait: %.1fs", currentTimer);
                }
            }
        }
        else if (node.type == EditorBTNodeType::Condition) {
            const char* condName = "Unknown";
            for (int i = 0; i < g_ConditionRegistryCount; ++i) {
                if (g_ConditionRegistry[i].id == node.actionOrConditionId) {
                    condName = g_ConditionRegistry[i].name; break;
                }
            }
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "? %s", condName);
        }
        // =======================================================
        //  デコレーターの表示とリアルタイム・プログレスバー
        // =======================================================
        else if (node.type == EditorBTNodeType::Decorator) {
            const char* decName = "Unknown";
            if (node.decoratorType == 0) decName = "Inverter (反転)";
            else if (node.decoratorType == 1) decName = "Cooldown (待機)";
            else if (node.decoratorType == 2) decName = "Retry (再試行)";

            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "* %s", decName);

            // デバッガー接続時（ゲーム実行中）のみの表示
            if (activeAI && node.runtimeFlatIndex >= 0 && node.runtimeFlatIndex < activeAI->nodeTimers.size()) {
                if (node.decoratorType == 1) { // Cooldownの場合
                    float currentTimer = activeAI->nodeTimers[node.runtimeFlatIndex];
                    if (currentTimer > 0.0f) {
                        // クールダウン中は赤文字で残り秒数を表示
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Cooling: %.1fs", currentTimer);

                        // プログレスバー（ゲージ）の描画
                        float maxTime = (std::max)(0.001f, node.decoratorParam); // ゼロ除算防止
                        float fraction = 1.0f - (currentTimer / maxTime); // 0.0(空) 〜 1.0(満タン)

                        // プログレスバーの色を赤っぽくする
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                        ImGui::ProgressBar(fraction, ImVec2(NODE_WIDTH - 20, 6), ""); // ゲージのみ表示
                        ImGui::PopStyleColor();
                    }
                    else {
                        // 発動可能な時は緑色で Ready と表示
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Ready");
                    }
                }
            }
        }

        ImGui::Unindent(10.0f);
        ImGui::Spacing();
        ImGui::EndGroup();



        // --- D. 下部出力バー（UE5スタイル） ---
        if (node.outputPin.id != 0) {
            ImGui::Spacing();
            ed::BeginPin(node.outputPin.id, ed::PinKind::Output);
            ImGui::Dummy(ImVec2(NODE_WIDTH, 20.0f)); // ★高さを12→20に拡大

            ImVec2 pMin = ImGui::GetItemRectMin();
            ImVec2 pMax = ImGui::GetItemRectMax();
            // 接続バーの描画（少し明るい色にする）
            drawList->AddRectFilled(ImVec2(pMin.x + 2, pMin.y + 2), ImVec2(pMax.x - 2, pMax.y - 6), ImColor(100, 100, 100), 2.0f);
            ed::EndPin();
        }

        ed::EndNode();
        // PopStyleVarを1から2に変更（NodeBorderWidthを追加したため）
        ed::PopStyleVar(2);
        ed::PopStyleColor(2);

        // 座標更新
        ImVec2 p = ed::GetNodePosition(node.id);
        node.posX = p.x;
        node.posY = p.y;
    }
    
   
    // =========================================================
    // ★ 血流の可視化（Execution Flow Debugging）
    // =========================================================
    for (auto& link : _links) {
        // デフォルトの管の状態（非アクティブ）
        ImVec4 linkColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 白
        float linkThickness = 2.0f; // 細い

        // 1. この管に血液を流し込んでいる「親ノード」を検索する
        auto parentNodeIt = std::find_if(_nodes.begin(), _nodes.end(), [&](const EditorBTNode& n) {
            return n.outputPin.id == link.startPinId;
            });

        if (parentNodeIt != _nodes.end()) {
            // 2. ★修正: ゲーム実行中(activeAI)かつBake済みのノードなら、ランタイムの配列から状態を取得する
            if (activeAI && parentNodeIt->runtimeFlatIndex >= 0 && parentNodeIt->runtimeFlatIndex < activeAI->debugNodeStates.size()) {

                // 正しい Enum である BTDebugState を取得
                BTDebugState state = activeAI->debugNodeStates[parentNodeIt->runtimeFlatIndex];

                // 3. 状態に応じた「血流エフェクト」の適用
                if (state == BTDebugState::Running) { // 実行中
                    float time = ImGui::GetTime();
                    float pulse = (std::sin(time * 10.0f) + 1.0f) * 0.5f;
                    float alpha = 0.5f + (pulse * 0.5f);

                    linkColor = ImVec4(1.0f, 0.6f, 0.0f, alpha);
                    linkThickness = 4.0f;
                }
                else if (state == BTDebugState::Success) { // 成功して通過
                    linkColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // 鮮やかな緑
                    linkThickness = 3.0f;
                }
                else if (state == BTDebugState::Failure) { // 失敗して遮断
                    linkColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // 鮮やかな赤
                    linkThickness = 3.0f;
                }
            }
        }

        // 4. 決定した色と太さでリンクを描画
        ed::Link(link.id, link.startPinId, link.endPinId, linkColor, linkThickness);
    }

    // =======================================================
    // ★ 究極の改修：実行中ノードへのスマート・オートフォロー
    // =======================================================
    if (_autoFollow && currentActiveNodeId != 0) {
        if (_lastTrackedNodeId != currentActiveNodeId) {

            // 1. 現在ユーザーが選択しているノード（インスペクタで見ているもの等）を記憶
            std::vector<ed::NodeId> selectedNodes;
            selectedNodes.resize(ed::GetSelectedObjectCount());
            int count = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
            selectedNodes.resize(count);

            // 2. 一瞬だけターゲットを単独選択する
            ed::ClearSelection();
            ed::SelectNode(currentActiveNodeId);

            // 3. 選択中のもの（＝ターゲット）へカメラを向ける命令を出す
            // false = ズームインしない（引き目を維持）, 0.4f = 0.4秒かけて滑らかに移動
            ed::NavigateToSelection(false, 0.4f);

            // 4. ユーザーの元の選択状態を即座に復元する（インスペクタの表示は切り替わらない）
            ed::ClearSelection();
            for (auto id : selectedNodes) {
                ed::SelectNode(id, true); // true = 追加選択
            }

            _lastTrackedNodeId = currentActiveNodeId;
        }
    }
    else if (currentActiveNodeId == 0) {
        _lastTrackedNodeId = 0; // 実行が止まったらリセット
    }


    if (ed::BeginCreate()) {
        ed::PinId startPinId = 0, endPinId = 0;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {
            if (startPinId && endPinId) {

                // 1. ピンの属性（InputかOutputか）を特定する
                bool startIsInput = false;
                bool endIsInput = false;
                for (const auto& n : _nodes) {
                    if (n.inputPin.id == startPinId.Get()) startIsInput = true;
                    if (n.inputPin.id == endPinId.Get()) endIsInput = true;
                }

                // 2. Input同士、Output同士の接続は赤線にして拒否する
                if (startIsInput == endIsInput) {
                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                }
                else {
                    if (ed::AcceptNewItem()) {
                        BTLink newLink;
                        newLink.id = GetNextId();

                        // 3. ★修正の要：ユーザーのドラッグ方向に依存せず、必ず startPinId に「出力ピン(親)」、endPinId に「入力ピン(子)」を入れる
                        if (startIsInput) {
                            newLink.startPinId = endPinId.Get();
                            newLink.endPinId = startPinId.Get();
                        }
                        else {
                            newLink.startPinId = startPinId.Get();
                            newLink.endPinId = endPinId.Get();
                        }

                        _links.push_back(newLink);
                        _executionOrderDirty = true;
                    }
                }
            }
        }
    }
    ed::EndCreate();

    // =======================================================
     //  プロフェッショナル向け便利ショートカット機能
     // =======================================================
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        // 2. Ctrl + D キーで選択ノードを瞬時に複製 (Duplicate) のみを残す
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            DuplicateSelectedNodes();
        }
    }


    bool itemsDeleted = false; 
    if (ed::BeginDelete()) {
        ed::NodeId deletedNodeId = 0;
        while (ed::QueryDeletedNode(&deletedNodeId)) {
            if (ed::AcceptDeletedItem()) {
                DeleteNodeAndLinks(deletedNodeId.Get());
                itemsDeleted = true; 
            }
        }
        ed::LinkId deletedLinkId = 0;
        while (ed::QueryDeletedLink(&deletedLinkId)) {
            if (ed::AcceptDeletedItem()) {
                _links.erase(std::remove_if(_links.begin(), _links.end(),
                    [&](const BTLink& l) { return l.id == deletedLinkId.Get(); }), _links.end());
                _executionOrderDirty = true; 
                itemsDeleted = true;         
            }
        }
    }
    ed::EndDelete();

    // ★追加: 削除が完了したらUndoスタックに記録する
    if (itemsDeleted) {
        RecordStateForUndo("Delete Node/Link");
    }

    HandleContextMenu();
    ed::End();

    if (_needInitialZoom) {
        ed::NavigateToContent();
        _needInitialZoom = false;
    }
}

// ====================================================================
// インスペクタ（右画面）の描画処理
// ====================================================================
void BehaviorTreeWindow::DrawInspector() {
    ImGui::Text("インスペクタ (Inspector)");
    ImGui::Separator();

    std::vector<ed::NodeId> selectedNodes;
    selectedNodes.resize(ed::GetSelectedObjectCount());
    int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));

    if (nodeCount > 0) {
        uint32_t selectedId = selectedNodes[0].Get();
        auto it = std::find_if(_nodes.begin(), _nodes.end(), [&](const EditorBTNode& n) { return n.id == selectedId; });

        if (it != _nodes.end()) {
            // =========================================================
            // Undoを安全に記録するための遅延フラグ
            // =========================================================
            bool needsUndo = false;
            std::string undoMessage = "Inspector Changed";


            // ★機能追加: ノード名の変更（コメントや整理に必須）
            char nameBuf[256];
            strcpy_s(nameBuf, it->name.c_str());
            if (ImGui::InputText("名前 (Name)", nameBuf, sizeof(nameBuf))) {
                it->name = nameBuf;
            }

            // ★ 入力が完了（エンターかフォーカス外れ）した時だけUndo記録
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                needsUndo = true;
                undoMessage = "Change Node Name";
            }

            // =========================================================
            // 最高のUXを実現する「ノードの取扱説明書」UI
            // =========================================================
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // 黄色テキスト
            ImGui::Text("[ノードの役割と使い方]");
            ImGui::PopStyleColor();

            // 自動折り返しを有効にして、ウィンドウ幅に合わせてテキストを改行させる
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x);

            switch (it->type) {
            case EditorBTNodeType::Selector:
                ImGui::Text("【Selector (セレクター / OR条件)】\n"
                    "子ノードを左から順に実行し、どれか1つでも「Success(成功)」すれば親にSuccessを返して終了します。\n"
                    "💡 使い方：優先度の高い行動（左）から順番に試す「行動の分岐点」として使います。");
                break;
            case EditorBTNodeType::Sequence:
                ImGui::Text("【Sequence (シーケンス / AND条件)】\n"
                    "子ノードを左から順に実行し、1つでも「Failure(失敗)」したら親にFailureを返して終了します。\n"
                    "💡 使い方：「条件チェック」→「近づく」→「攻撃する」といった、一連のアクションを順番にこなすセットを作るのに使います。");
                break;
            case EditorBTNodeType::Condition:
                ImGui::Text("【Condition (条件)】\n"
                    "状況（距離やHPなど）を判定し、SuccessかFailureを即座に返します。\n"
                    "💡 使い方：Sequenceの先頭（一番左）に置いて、その行動セットを実行してよいかの「門番」として使います。");
                break;
            case EditorBTNodeType::Action:
                ImGui::Text("【Action (アクション / 行動)】\n"
                    "実際にAIを動かす末端の命令です。ゲームのロジックに命令を送ります。\n"
                    "💡 使い方：移動や攻撃など。実行に時間がかかる行動は「Running(実行中)」を返し続け、次フレームも呼ばれます。");
                break;
            case EditorBTNodeType::Decorator:
                ImGui::Text("【Decorator (デコレーター / 修飾)】\n"
                    "下に1つだけ子ノードを持ち、その子の結果や実行タイミングを操作します。\n"
                    "💡 使い方：強力な攻撃に「10秒のクールダウン」をつけたり、失敗時に「2回リトライ」させたりする制限をかけるのに使います。");
                break;
            case EditorBTNodeType::Group:
                ImGui::Text("【Group (グループ / コメント)】\n"
                    "複数のノードを視覚的に囲んで整理するための枠です。\n"
                    "💡 使い方：AIの実行ロジックには一切影響を与えません。メモや整理に使ってください。");
                break;
            }
            ImGui::PopTextWrapPos(); // 自動折り返し終了
            ImGui::Separator();

            // ---------------------------------------------------------
            // 各種プロパティの編集とUndoフラグのセット
            // ---------------------------------------------------------
            if (it->type == EditorBTNodeType::Condition) {
                ImGui::Separator();
                ImGui::Text("条件設定 (Condition):");
                int currentIndex = 0;
                const char* names[64];
                for (int i = 0; i < g_ConditionRegistryCount; ++i) {
                    names[i] = g_ConditionRegistry[i].name;
                    if (g_ConditionRegistry[i].id == it->actionOrConditionId) currentIndex = i;
                }
                if (ImGui::Combo("条件を選ぶ", &currentIndex, names, g_ConditionRegistryCount)) {
                    it->actionOrConditionId = g_ConditionRegistry[currentIndex].id;
                    _executionOrderDirty = true;
                    needsUndo = true;
                    undoMessage = "Change Condition";
                }
            }
            else if (it->type == EditorBTNodeType::Action) {
                ImGui::Separator();
                ImGui::Text("行動設定 (Action):");
                int currentIndex = 0;
                const char* names[64];
                for (int i = 0; i < g_ActionRegistryCount; ++i) {
                    names[i] = g_ActionRegistry[i].name;
                    if (g_ActionRegistry[i].id == it->actionOrConditionId) currentIndex = i;
                }
                if (ImGui::Combo("行動を選ぶ", &currentIndex, names, g_ActionRegistryCount)) {
                    it->actionOrConditionId = g_ActionRegistry[currentIndex].id;
                    _executionOrderDirty = true;
                    needsUndo = true;
                    undoMessage = "Change Action";
                }
            }

            if (it->type == EditorBTNodeType::Decorator) {
                ImGui::Separator();
                ImGui::Text("デコレーター設定 (Decorator):");

                const char* decNames[] = { "0: 結果反転 (Inverter)", "1: クールダウン (Cooldown)", "2: リトライ (Retry)", "3: 非同期 (Async)" }; 
                if (ImGui::Combo("タイプ", &it->decoratorType, decNames, IM_ARRAYSIZE(decNames))) {
                    _executionOrderDirty = true;
                    needsUndo = true;
                    undoMessage = "Change Decorator Type";
                }

                if (it->decoratorType == 1) { // Cooldown
                    // ★ ドラッグ中は数値を変えるだけ
                    ImGui::DragFloat("待機時間 (秒)", &it->decoratorParam, 0.1f, 0.0f, 60.0f);

                    // ★ マウスを離した時だけUndo記録
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        needsUndo = true;
                        undoMessage = "Change Cooldown Time";
                    }
                }
                else if (it->decoratorType == 2) { // Retry
                    int retryCount = static_cast<int>(it->decoratorParam);
                    if (ImGui::DragInt("リトライ回数", &retryCount, 1, 1, 10)) {
                        it->decoratorParam = static_cast<float>(retryCount);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        needsUndo = true;
                        undoMessage = "Change Retry Count";
                    }
                }
            }


            // =========================================================
            // ★ 修正の要：すべての `it` の使用が終わった「一番最後」にUndoを記録する！
            // これによりイテレータ無効化によるクラッシュを完全に防ぐ
            // =========================================================
            if (needsUndo) {
                RecordStateForUndo(undoMessage);
            }
        }
    }
    else {
        ImGui::TextDisabled("編集するノードを選択してください。");
    }
}


// ====================================================================
// ノードと関連リンクの安全な連鎖削除
// ====================================================================
void BehaviorTreeWindow::DeleteNodeAndLinks(uint32_t nodeId) {
    // 1. 削除対象のノードを探す
    auto it = std::find_if(_nodes.begin(), _nodes.end(), [nodeId](const EditorBTNode& n) { return n.id == nodeId; });
    if (it == _nodes.end()) return;

    // 2. そのノードが持っているピンのIDを取得
    uint32_t inPinId = it->inputPin.id;
    uint32_t outPinId = it->outputPin.id;

    // 3. このピンに繋がっているリンクを全て配列から消し去る
    _links.erase(std::remove_if(_links.begin(), _links.end(),
        [inPinId, outPinId](const BTLink& link) {
            return link.startPinId == outPinId || link.endPinId == inPinId;
        }), _links.end());

    // 4. 最後にノード本体を消す
    _nodes.erase(it);

    _executionOrderDirty = true;

    CCL_LOG_INFO(LogCategory::Editor, "Node(ID:%u) and its connected links were deleted.", nodeId);
}

// ====================================================================
// 右クリックコンテキストメニュー処理
// ====================================================================
void BehaviorTreeWindow::HandleContextMenu() {
    ed::Suspend();

    // =======================================================
    //  Spaceキーでマウスカーソルの位置にメニューを出す
    // =======================================================
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Space)) {
        ImGui::OpenPopup("Create New Node");
        _newNodeSpawnPos = ImGui::GetMousePos(); // Spaceを押した瞬間のマウス座標を記憶
    }

    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("Create New Node");
        _newNodeSpawnPos = ImGui::GetMousePosOnOpeningCurrentPopup();
    }

    if (ImGui::BeginPopup("Create New Node")) {
        ImVec2 canvasPos = ed::ScreenToCanvas(_newNodeSpawnPos);

        if (ImGui::MenuItem("セレクター (Selector)")) CreateNode(EditorBTNodeType::Selector, "セレクター (Selector)", canvasPos.x, canvasPos.y);
        if (ImGui::MenuItem("シーケンス (Sequence)")) CreateNode(EditorBTNodeType::Sequence, "シーケンス (Sequence)", canvasPos.x, canvasPos.y);
        ImGui::Separator();

        if (ImGui::MenuItem("デコレーター (Decorator)")) CreateNode(EditorBTNodeType::Decorator, "デコレーター (Decorator)", canvasPos.x, canvasPos.y);
        if (ImGui::MenuItem("コンディション (Condition)")) CreateNode(EditorBTNodeType::Condition, "コンディション (Condition)", canvasPos.x, canvasPos.y);
        if (ImGui::MenuItem("アクション (Action)")) CreateNode(EditorBTNodeType::Action, "アクション (Action)", canvasPos.x, canvasPos.y);
        ImGui::Separator();
        // グループノード生成
        if (ImGui::MenuItem("コメントグループ (Group)")) CreateNode(EditorBTNodeType::Group, "コメント (Comment)", canvasPos.x, canvasPos.y);

        ImGui::Separator();
        // 複数ノードの複製
        if (ImGui::MenuItem("選択ノードを複製 (Duplicate)")) {
            DuplicateSelectedNodes();
        }

        ImGui::EndPopup();
    }

    ed::Resume();
}

// ====================================================================
// セーブ処理：ピンIDを明示的に保存するように拡張
// ====================================================================
void BehaviorTreeWindow::SaveGraph(const std::string& path) {
    json root;
    ed::SetCurrentEditor(_nodeContext);

    // ノード情報の保存
    json jNodes = json::array();
    for (auto& node : _nodes) {
        json jn;
        jn["id"] = node.id;
        jn["type"] = static_cast<int>(node.type);
        jn["name"] = node.name;
        jn["actionId"] = static_cast<int>(node.actionOrConditionId);

        // ピンのIDを保存。これがリンクの復元に不可欠。
        jn["inputPinId"] = node.inputPin.id;
        jn["outputPinId"] = node.outputPin.id;

        ImVec2 pos = ed::GetNodePosition(node.id);
        jn["posX"] = pos.x;
        jn["posY"] = pos.y;
        // グループサイズを保存
        if (node.type == EditorBTNodeType::Group) {
            jn["width"] = node.width;
            jn["height"] = node.height;
        }
        // デコレーターのパラメータを保存する
        if (node.type == EditorBTNodeType::Decorator) {
            jn["decoratorType"] = node.decoratorType;
            jn["decoratorParam"] = node.decoratorParam;
        }

        jNodes.push_back(jn);
    }
    root["editor_nodes"] = jNodes;

    // リンク情報の保存
    json jLinks = json::array();
    for (auto& link : _links) {
        json jl;
        jl["id"] = link.id;
        jl["startPin"] = link.startPinId;
        jl["endPin"] = link.endPinId;
        jLinks.push_back(jl);
    }
    root["editor_links"] = jLinks;
    root["last_id_counter"] = _idCounter;

    // =======================================================
    //  ここで BuildBakedNodes() を呼び出して統合する
    // =======================================================
    root["nodes"] = BuildBakedNodes();
    root["assetId"] = 1;

    // 書き出し（1回だけファイルを開く）
    std::ofstream o(path);
    if (o.is_open()) {
        o << root.dump(4);
        CCL_LOG_SUCCESS(LogCategory::Editor, "AIグラフを保存＆Bakeしました: %s", path.c_str());
    }

}

// =========================================================
// セーブ処理の統合 (上書きバグ解消 / 新children配列対応版)
// =========================================================
nlohmann::json BehaviorTreeWindow::BuildBakedNodes() {
    nlohmann::json jsonNodes = nlohmann::json::array();
    if (_nodes.empty()) return jsonNodes;

    // 1. Rootノードを探す
    EditorBTNode* rootNode = nullptr;
    for (auto& n : _nodes) {
        if (n.type == EditorBTNodeType::Root) { rootNode = &n; break; }
    }
    if (!rootNode) return jsonNodes;

    // ヘルパー：特定の出力ピンから繋がっている子ノードを「X座標順」に取得する
    auto getSortedChildren = [&](uint32_t outputPinId) {
        std::vector<EditorBTNode*> children;
        for (const auto& link : _links) {
            if (link.startPinId == outputPinId) {
                for (auto& n : _nodes) {
                    if (n.inputPin.id == link.endPinId) {
                        children.push_back(&n); break;
                    }
                }
            }
        }
        std::sort(children.begin(), children.end(), [](EditorBTNode* a, EditorBTNode* b) {
            return a->posX < b->posX;
            });
        return children;
        };

    auto rootChildren = getSortedChildren(rootNode->outputPin.id);
    if (rootChildren.empty()) return jsonNodes;

    // 2. BFS（幅優先探索）によるDODフラット配列の構築
    struct BakeTask { EditorBTNode* editorNode; int flatIndex; };
    std::queue<BakeTask> tasks;

    auto getRuntimeTypeString = [](EditorBTNodeType type) {
        switch (type) {
        case EditorBTNodeType::Selector:  return "Selector";
        case EditorBTNodeType::Sequence:  return "Sequence";
        case EditorBTNodeType::Condition: return "Condition";
        case EditorBTNodeType::Action:    return "Action";
        case EditorBTNodeType::Decorator: return "Decorator";
        default: return "Unknown";
        }
        };

    // =======================================================
    // ユーザーがROOTに複数繋いでしまった場合の警告
    // =======================================================
    if (rootChildren.size() > 1) {
        CCL_LOG_ERROR(LogCategory::Editor, "【BTエラー】ROOTノードには複数の線を繋げません！必ず1つの「Selector」等に繋いでから分岐させてください。");
    }

    EditorBTNode* actualRoot = rootChildren[0];

    nlohmann::json rootJson;
    rootJson["type"] = getRuntimeTypeString(actualRoot->type);
    rootJson["actionOrConditionId"] = static_cast<int>(actualRoot->actionOrConditionId);
    rootJson["children"] = nlohmann::json::array(); // ★修正: 配列として初期化
    jsonNodes.push_back(rootJson);

    tasks.push({ actualRoot, 0 });

    while (!tasks.empty()) {
        BakeTask task = tasks.front();
        tasks.pop();

        auto children = getSortedChildren(task.editorNode->outputPin.id);
        if (children.empty()) continue;

        // 子供たちを配列に追加し、次のキューへ
        for (EditorBTNode* child : children) {
            nlohmann::json childJson;
            childJson["type"] = getRuntimeTypeString(child->type);
            childJson["actionOrConditionId"] = static_cast<int>(child->actionOrConditionId);
            childJson["children"] = nlohmann::json::array(); // ★修正: 配列として初期化

            // デコレーター情報のBake
            if (child->type == EditorBTNodeType::Decorator) {
                childJson["decoratorType"] = child->decoratorType;
                childJson["decoratorParam"] = child->decoratorParam;
            }

            child->runtimeFlatIndex = jsonNodes.size();

            // ★修正の要：親ノードの "children" 配列に、この子のインデックスを追加する
            jsonNodes[task.flatIndex]["children"].push_back(jsonNodes.size());

            jsonNodes.push_back(childJson);
            tasks.push({ child, static_cast<int>(jsonNodes.size() - 1) });
        }
    }

    return jsonNodes;
}



// ====================================================================
// ロード処理：ピンIDを生成せず、保存された値で復元する
// ====================================================================
void BehaviorTreeWindow::LoadGraph(const std::string& path) {
    std::ifstream i(path);
    if (!i.is_open()) return;

    json j;
    i >> j;

    _nodes.clear();
    _links.clear();
    _idCounter = j.value("last_id_counter", 1);

    // ノードの復元
    for (auto& jn : j["editor_nodes"]) {
        EditorBTNode node;
        node.id = jn["id"];
        node.type = static_cast<EditorBTNodeType>(jn["type"].get<int>());
        node.name = jn["name"];
        node.actionOrConditionId = static_cast<ActionID>(jn["actionId"].get<int>());
        node.posX = jn["posX"];
        node.posY = jn["posY"];
        // ★追加: グループサイズを復元
        if (node.type == EditorBTNodeType::Group) {
            node.width = jn.value("width", 300.0f);
            node.height = jn.value("height", 200.0f);
        }

        // =======================================================
        // ★ 追加：デコレーターのパラメータを復元する
        // =======================================================
        if (node.type == EditorBTNodeType::Decorator) {
            node.decoratorType = jn.value("decoratorType", 0);
            node.decoratorParam = jn.value("decoratorParam", 0.0f);
        }

        // ★ 修正：GetNextId() を使わず、保存されたIDをそのままセットする
        node.inputPin = { jn.value("inputPinId", 0u), node.id, true };
        node.outputPin = { jn.value("outputPinId", 0u), node.id, false };

        _nodes.push_back(node);
    }

    // リンクの復元
    for (auto& jl : j["editor_links"]) {
        BTLink link;
        link.id = jl["id"];
        link.startPinId = jl["startPin"];
        link.endPinId = jl["endPin"];
        _links.push_back(link);
    }

    // =======================================================
    // ロード完了直後に「空回し」のBakeを実行する！
    // これにより、わざわざ保存しなくても `runtimeFlatIndex` が計算され、
    // ロードした瞬間からリアルタイムデバッグが可能になる。
    // =======================================================
    BuildBakedNodes();

    _needInitialZoom = true;
    CCL_LOG_SUCCESS(LogCategory::Editor, "AIグラフをロードしました: %s", path.c_str());
}

// ====================================================================
// 選択されたノードと、その間のリンクをまとめて複製する高度な機能
// ====================================================================
void BehaviorTreeWindow::DuplicateSelectedNodes() {
    std::vector<ed::NodeId> selectedNodeIds;
    selectedNodeIds.resize(ed::GetSelectedObjectCount());
    int nodeCount = ed::GetSelectedNodes(selectedNodeIds.data(), static_cast<int>(selectedNodeIds.size()));

    if (nodeCount == 0) return;

    // 古いIDから新しいIDへのマッピングを記録（リンクの再接続に使う）
    std::map<uint32_t, uint32_t> oldToNewPinId;

    std::vector<EditorBTNode> newNodes;

    // 1. ノードの複製
    for (int i = 0; i < nodeCount; ++i) {
        uint32_t oldId = selectedNodeIds[i].Get();
        auto it = std::find_if(_nodes.begin(), _nodes.end(), [oldId](const EditorBTNode& n) { return n.id == oldId; });

        if (it != _nodes.end()) {
            EditorBTNode newNode = *it;
            newNode.id = GetNextId();
            newNode.posX += 50.0f; // 重ならないように右下に少しずらす
            newNode.posY += 50.0f;
            newNode.positionInitialized = false;
            newNode.executionOrder = -1;

            // 新しいピンIDの割り当てとマッピング
            if (newNode.inputPin.id != 0) {
                uint32_t newIn = GetNextId();
                oldToNewPinId[newNode.inputPin.id] = newIn;
                newNode.inputPin.id = newIn;
                newNode.inputPin.nodeId = newNode.id;
            }
            if (newNode.outputPin.id != 0) {
                uint32_t newOut = GetNextId();
                oldToNewPinId[newNode.outputPin.id] = newOut;
                newNode.outputPin.id = newOut;
                newNode.outputPin.nodeId = newNode.id;
            }

            newNodes.push_back(newNode);
        }
    }

    // 2. リンクの複製（複製されたノード同士が繋がっていた場合のみ）
    std::vector<BTLink> newLinks;
    for (const auto& link : _links) {
        if (oldToNewPinId.count(link.startPinId) && oldToNewPinId.count(link.endPinId)) {
            BTLink newLink;
            newLink.id = GetNextId();
            newLink.startPinId = oldToNewPinId[link.startPinId];
            newLink.endPinId = oldToNewPinId[link.endPinId];
            newLinks.push_back(newLink);
        }
    }

    // 3. コピーした要素を世界に反映
    _nodes.insert(_nodes.end(), newNodes.begin(), newNodes.end());
    _links.insert(_links.end(), newLinks.begin(), newLinks.end());

    // 操作完了後、選択状態を解除する
    ed::ClearSelection();
    CCL_LOG_INFO(LogCategory::Editor, "%d nodes duplicated.", nodeCount);
}

