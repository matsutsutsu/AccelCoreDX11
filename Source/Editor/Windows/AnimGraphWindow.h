#pragma once
#include "Editor/Core/EditorWindow.h"
#include "Engine/GamePlay/Animation/Data/AnimStateMachine.h"
#include <string>

namespace ax { namespace NodeEditor { struct EditorContext; } }

class AnimGraphWindow : public EditorWindow {
public:
    AnimGraphWindow();
    virtual ~AnimGraphWindow();

protected:
    void DrawContents(EditorContext& context) override;

private:
    void DrawToolbar();
    void DrawNodeWorkspace();
    void DrawInspector();

    // インスペクタの描画を役割ごとに分割
    void DrawStateSettings(uint32_t stateHash);
    void DrawTransitionSettings(int linkId);

    // Transition の共通UI描画
    void DrawTransitionConditionUI(AnimTransition& trans);

    void HandleInteraction();

    void LoadGraph(const std::string& path);
    void SaveGraph(const std::string& path);

private:
    AnimStateGraph _currentGraph;
    std::string _currentFilePath = "Assets/Animations/PlayerAnimGraph.json";

    ax::NodeEditor::EditorContext* _nodeContext = nullptr;

    // ノードとリンクの選択状態を独立して管理
    uint32_t _selectedNodeHash = 0;
    int _selectedLinkId = 0;

    uint32_t _nodeToPlaceHash = 0;
    char _newStateName[64] = "";

    // ロード直後に座標を復元するためのフラグ
    bool _needRestorePositions = false;

    // 右クリックした瞬間のスクリーン座標を記憶する変数
    ImVec2 _newNodeSpawnPos = { 0.0f, 0.0f };

    // コンテキストメニューを処理する関数の宣言
    void HandleContextMenu();

    // =====================================================
    // デバッグ用の「現在実行中のステート」を保持する変数
    // =====================================================
    uint32_t _runtimeActiveStateHash = 0;
};