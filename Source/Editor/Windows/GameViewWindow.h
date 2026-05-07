#pragma once
#include "Editor/Core/EditorWindow.h"

class GameViewWindow : public EditorWindow {
public:
    GameViewWindow() : EditorWindow("Game View") {}

protected:
    void DrawContents(EditorContext& context) override {
        // EditorSceneが保持しているSRVをContext経由で取得できるように拡張する必要があります
        // ここでは一旦、描画領域の確保のみ
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // 16:9計算（EditorScene.cppから移植）
        const float targetAspect = 16.0f / 9.0f;
        float windowAspect = avail.x / avail.y;
        ImVec2 imageSize = (windowAspect > targetAspect)
            ? ImVec2(avail.y * targetAspect, avail.y)
            : ImVec2(avail.x, avail.x / targetAspect);

        // context.gameSRV など、テクスチャへのポインタが必要です
        // ImGui::Image((void*)context.gameSRV, imageSize);

        ImGui::Text("Render Texture Area: %.1fx%.1f", imageSize.x, imageSize.y);
    }
};