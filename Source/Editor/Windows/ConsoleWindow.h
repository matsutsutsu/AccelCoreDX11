#pragma once
#include "Editor/Core/EditorWindow.h"
#include "Engine/Platform/Logger.h"

class ConsoleWindow : public EditorWindow {
public:
    ConsoleWindow();

protected:
    void DrawContents(EditorContext& context) override;

private:
    bool _showInfo = true;
    bool _showWarning = true;
    bool _showError = true;
    bool _autoScroll = true; // 新しいログが出たら自動で一番下にスクロールするか

    // カテゴリごとの表示フラグ（全カテゴリ最初はON）
    // LogCategory::Count を配列のサイズに使うプロのテクニックです
    bool _categoryFilters[(int)LogCategory::Count] = {
			true, true, true, true, true, true, true, true
    };

};