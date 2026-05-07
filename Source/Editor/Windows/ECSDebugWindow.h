#pragma once
#include "Editor/Core/EditorWindow.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemManager.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <map>

// =========================================================
// ★ プロファイラー用の履歴保持バッファ (リングバッファ)
// =========================================================
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;

    ScrollingBuffer(int max_size = 600);
    void AddPoint(float x, float y);
};

struct SystemProfileData {
    ScrollingBuffer buffer;
    ImVec4 color = ImVec4(1, 1, 1, 1); // 線の色
    bool isVisible = true;             // グラフに表示するかどうか
    bool isHovered = false;            // テーブル上でマウスが乗っているか
};

class ECSDebugWindow : public EditorWindow {
private:
    std::map<std::string, SystemProfileData> _systemTimings;
    float _time = 0.0f;
    bool  _pauseProfiler = false;

public:
    ECSDebugWindow();

protected:
    void DrawContents(EditorContext& context) override;

private:
    // 各タブの描画処理を分割
    void DrawMemoryTab(CCL::ECS::Core::World* world);
    void DrawSystemsTab(CCL::ECS::SystemManager* systemManager);
    void DrawPendingOpsTab(CCL::ECS::Core::World* world);
    // 実行グラフ描画用メソッド
    void DrawExecutionGraphTab(CCL::ECS::SystemManager* systemManager);

    // 型IDから名前を取得するヘルパー関数の宣言
    const char* GetFriendlyTypeName(uint32_t id);
};