#include "NavMeshWindow.h"
#include <imgui.h>

NavMeshWindow::NavMeshWindow() : EditorWindow("NavMesh Builder") {}

void NavMeshWindow::DrawContents(EditorContext& context)
{
    // 世界が存在しない（ロード前など）場合は何も描画・実行しない
    if (!context.world) return;

    ImGui::SeparatorText("Agent Settings");

    // =======================================================
    // 1. 基本パラメータ (エージェントのサイズや移動能力)
    // =======================================================
    ImGui::DragFloat("Agent Radius (太さ)", &_navSettings.agentRadius, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("Agent Height (身長)", &_navSettings.agentHeight, 0.1f, 0.1f, 5.0f);
    ImGui::DragFloat("Max Climb (段差)", &_navSettings.agentMaxClimb, 0.05f, 0.0f, 2.0f);
    ImGui::DragFloat("Max Slope (斜面角度)", &_navSettings.agentMaxSlope, 1.0f, 0.0f, 90.0f);

    ImGui::Spacing();

    // =======================================================
    // 2. 詳細パラメータ (ボクセルの解像度など)
    // ※普段は隠しておき、精度を追い込みたい時だけ開く設計
    // =======================================================
    if (ImGui::CollapsingHeader("Advanced Settings")) {
        ImGui::DragFloat("Cell Size (XY解像度)", &_navSettings.cellSize, 0.05f, 0.1f, 1.0f);
        ImGui::DragFloat("Cell Height (Z解像度)", &_navSettings.cellHeight, 0.05f, 0.1f, 1.0f);
        ImGui::DragFloat("Region Min Size", &_navSettings.regionMinSize, 1.0f, 0.0f, 50.0f);
        ImGui::DragFloat("Region Merge Size", &_navSettings.regionMergeSize, 1.0f, 0.0f, 50.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =======================================================
    // 3. Bake 実行ボタン
    // =======================================================
    // ボタンの色を少し目立たせる（UnrealのBuildボタンのようなUX）
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

    if (ImGui::Button("Bake NavMesh", ImVec2(-1, 40))) {
        // コンテキストから最新のWorldを渡し、Bakeを実行する
        // ※内部で以前のNavMeshメモリは解放(dtFree)されるため、何度押しても安全
        NavMeshBuilder::BuildNavMesh(*context.world, _navSettings);
    }

    ImGui::PopStyleColor(3);
}