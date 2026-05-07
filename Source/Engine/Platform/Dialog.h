#pragma once

#include <Windows.h>

// ダイアログリザルト
enum class DialogResult
{
	Yes,
	No,
	OK,
	Cancel
};

// ダイアログ
class Dialog
{
public:
	// [ファイルを開く]ダイアログボックスを表示
	static DialogResult OpenFileName(char* filepath, int size,
		const char* filter = nullptr,
		const char* title = nullptr,
		const char* initialDir = nullptr, 
		HWND hWnd = NULL,
		bool multiSelect = false);

	// [ファイルを保存]ダイアログボックスを表示
	static DialogResult SaveFileName(char* filepath, int size,
		const char* filter = nullptr,
		const char* title = nullptr,
		const char* initialDir = nullptr, // ← これを追加
		const char* ext = nullptr,
		HWND hWnd = NULL);

	// 履歴の保存と読み込み
	static void LoadHistory(const char* settingsFilePath = "Build/EditorPrefs.json");
	static void SaveHistory(const char* settingsFilePath = "Build/EditorPrefs.json");
};
