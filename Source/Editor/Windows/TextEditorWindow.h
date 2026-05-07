#pragma once
#include "Editor/Core/EditorWindow.h"
#include <vector>

class TextEditorWindow : public EditorWindow {
  public:
    TextEditorWindow();
    virtual ~TextEditorWindow() = default;

  protected:
    void DrawContents(EditorContext &context) override;

  private:
    void LoadFile(const char *path);
    void SaveFile(const char *path);

  private:
    char       _filePath[256];
    std::vector<char> _textBuffer; // テキスト編集用の巨大バッファ
};