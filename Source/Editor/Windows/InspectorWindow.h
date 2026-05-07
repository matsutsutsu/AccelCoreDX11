#pragma once
#include "Editor/Core/EditorWindow.h"

class InspectorWindow : public EditorWindow {
  public:
    InspectorWindow();

  protected:
    void DrawContents(EditorContext &context) override;

   private:
    // コンポーネント検索用のバッファ
    char _compSearchBuffer[256] = {0};
};