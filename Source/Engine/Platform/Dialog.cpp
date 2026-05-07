#include "Dialog.h"
#include <unordered_map>
#include <string>
#include <cstdio> // sprintf_s用
#include <fstream>
#include <json.hpp>

// =========================================================================
// Shift-JIS と UTF-8 の相互変換ヘルパー
// =========================================================================
static std::string SjisToUtf8(const std::string& sjis) {
    if (sjis.empty()) return "";
    int wlen = MultiByteToWideChar(CP_ACP, 0, sjis.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, sjis.c_str(), -1, &wstr[0], wlen);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], u8len, nullptr, nullptr);

    utf8.resize(strlen(utf8.c_str())); // 末尾のNULL文字を除去
    return utf8;
}

static std::string Utf8ToSjis(const std::string& utf8) {
    if (utf8.empty()) return "";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wlen);

    int sjislen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string sjis(sjislen, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &sjis[0], sjislen, nullptr, nullptr);

    sjis.resize(strlen(sjis.c_str()));
    return sjis;
}

// ============================================================================
// 「ダイアログのタイトル」ごとに別々の履歴を記憶する辞書
// ============================================================================
static std::unordered_map<std::string, std::string> s_pathHistory;

// [ファイルを開く]ダイアログボックスを表示
DialogResult Dialog::OpenFileName(char* filepath, int size, const char* filter, const char* title, const char* initialDir, HWND hWnd, bool multiSelect)
{
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = filepath;
    ofn.nMaxFile = size;
    ofn.lpstrFilter = filter;
    ofn.lpstrTitle = title;
    ofn.nFilterIndex = 1;

    // ★必須修正: OFN_NOCHANGEDIR を絶対に入れること！
    // カレントディレクトリが勝手に変わるのを防ぎます。
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (multiSelect) {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }

    // ==============================================================
    // ★ 履歴と初期フォルダのスマートな解決ロジック
    // ダイアログの「タイトル」をキーにして履歴を独立させます。
    // ==============================================================
    std::string historyKey = title ? title : "DefaultOpen";

    // 優先順位: 1. 指定された初期フォルダ -> 2. 用途別の履歴 -> 3. 空(カレント)
    if (initialDir != nullptr) {
        // ★ プログラムで明示的にパスが指定されている場合は、絶対にそこを開く（履歴無視）
        ofn.lpstrInitialDir = initialDir;
    }
    else if (s_pathHistory.count(historyKey) > 0) {
        // ★ パスが指定されていない場合のみ、前回の履歴を使う
        ofn.lpstrInitialDir = s_pathHistory[historyKey].c_str();
    }

    // ダイアログを開く
    if (GetOpenFileNameA(&ofn)) {
        // 選ばれたファイルのディレクトリ部分だけを抽出して履歴に保存
        char drive[MAX_PATH], dir[MAX_PATH];
        _splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);

        char outDir[MAX_PATH];
        sprintf_s(outDir, "%s%s", drive, dir);

        s_pathHistory[historyKey] = outDir; // ★ タイトルごとに履歴を記憶

        return DialogResult::OK;
    }

    return DialogResult::Cancel;
}

// ※ SaveFileName 側も同様に initialDir と OFN_NOCHANGEDIR、historyKey の処理を追加してください。


// [ファイルを保存]ダイアログボックスを表示
DialogResult Dialog::SaveFileName(char* filepath, int size, const char* filter, const char* title, const char* initialDir, const char* ext, HWND hWnd)
{
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = filepath;
    ofn.nMaxFile = size;
    ofn.lpstrFilter = filter;
    ofn.lpstrTitle = title;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = ext;

    // ★必須修正: OFN_NOCHANGEDIR を絶対に入れること！
    // 保存時の上書き警告（OFN_OVERWRITEPROMPT）も標準で入れます。
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // ==============================================================
    // ★ 履歴と初期フォルダのスマートな解決ロジック
    // ダイアログの「タイトル」をキーにして履歴を独立させます。
    // ==============================================================
    std::string historyKey = title ? title : "DefaultSave";

    // 優先順位: 1. 指定された初期フォルダ -> 2. 用途別の履歴 -> 3. 空(カレント)
    if (initialDir != nullptr) {
        // ★ プログラムで明示的にパスが指定されている場合は、絶対にそこを開く（履歴無視）
        ofn.lpstrInitialDir = initialDir;
    }
    else if (s_pathHistory.count(historyKey) > 0) {
        // ★ パスが指定されていない場合のみ、前回の履歴を使う
        ofn.lpstrInitialDir = s_pathHistory[historyKey].c_str();
    }

    // 保存ダイアログを開く
    if (GetSaveFileNameA(&ofn)) {
        // 選ばれたファイルのディレクトリ部分だけを抽出して履歴に保存
        char drive[MAX_PATH], dir[MAX_PATH];
        _splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);

        char outDir[MAX_PATH];
        sprintf_s(outDir, "%s%s", drive, dir);

        s_pathHistory[historyKey] = outDir; // ★ タイトルごとに履歴を記憶

        return DialogResult::OK;
    }

    return DialogResult::Cancel;
}

// 履歴のロード
void Dialog::LoadHistory(const char* settingsFilePath)
{
    std::ifstream file(settingsFilePath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("DialogHistory")) {
                for (auto& item : j["DialogHistory"].items()) {
                    // JSONのUTF-8を、システム用のShift-JISに戻して記憶する
                    s_pathHistory[item.key()] = Utf8ToSjis(item.value().get<std::string>());
                }
            }
        }
        catch (...) {
            // ファイルが壊れていても無視してデフォルトで起動する
        }
    }
}

// 履歴のセーブ
void Dialog::SaveHistory(const char* settingsFilePath)
{
    nlohmann::json j;

    // 既存のファイルを読み込んで追記モードにする（他のエディタ設定を消さないため）
    std::ifstream inFile(settingsFilePath);
    if (inFile.is_open()) {
        try { inFile >> j; }
        catch (...) {}
        inFile.close();
    }

    // 履歴をJSONに書き込む
    // システムが保持しているShift-JISのパスを、UTF-8に変換してからJSONに書き込む
    for (const auto& pair : s_pathHistory) {
        j["DialogHistory"][pair.first] = SjisToUtf8(pair.second);
    }

    // 整形して保存
    std::ofstream outFile(settingsFilePath);
    if (outFile.is_open()) {
        outFile << std::setw(4) << j << std::endl;
    }
}