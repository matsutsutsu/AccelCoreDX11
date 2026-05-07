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
DialogResult Dialog::OpenFileName(char* filepath, int size, const char* filter, const char* title, HWND hWnd, bool multiSelect)
{
    // 履歴を分けるためのキー（タイトルがない場合は "Default"）
    std::string historyKey = title ? title : "Default";

    // 初期パス設定
    char dirname[MAX_PATH];

    // ★バグ修正: '0' ではなく '\0' (NULL文字) で正しく空文字判定を行う
    if (filepath[0] != '\0')
    {
        // 渡されたファイルパスからディレクトリ部分だけを抽出
        char drive[MAX_PATH];
        char dir[MAX_PATH];
        ::_splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);
        sprintf_s(dirname, "%s%s", drive, dir);
    }
    else
    {
        dirname[0] = '\0';
    }

    // パスが指定されていなければ、履歴から復元する
    if (dirname[0] == '\0')
    {
        // 過去に同じタイトルのダイアログを開いた履歴があればそれを使う
        if (s_pathHistory.find(historyKey) != s_pathHistory.end())
        {
            strcpy_s(dirname, MAX_PATH, s_pathHistory[historyKey].c_str());
        }
    }

    // lpstrInitialDir は \ でないと受け付けないため置換
    for (char* p = dirname; *p != '\0'; p++)
    {
        if (*p == '/')
            *p = '\\';
    }

    if (filter == nullptr)
    {
        filter = "All Files\0*.*\0\0";
    }

    // 構造体セット
    OPENFILENAMEA   ofn;
    memset(&ofn, 0, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = filepath;
    ofn.nMaxFile = size;
    ofn.lpstrTitle = title;
    ofn.lpstrInitialDir = (dirname[0] != '\0') ? dirname : nullptr;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (multiSelect)
    {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }

    // カレントディレクトリ取得
    char currentDir[MAX_PATH];
    if (!::GetCurrentDirectoryA(MAX_PATH, currentDir))
    {
        currentDir[0] = '\0';
    }

    // ダイアログオープン
    if (::GetOpenFileNameA(&ofn) == FALSE)
    {
        return DialogResult::Cancel;
    }

    // カレントディレクトリ復帰
    if (currentDir[0] != '\0')
    {
        ::SetCurrentDirectoryA(currentDir);
    }

    // ============================================================================
    // ★修正: 選ばれたファイルの「ディレクトリ部分」だけを抽出して履歴に保存する
    // ============================================================================
    char drive[MAX_PATH];
    char dir[MAX_PATH];
    ::_splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);

    char outDir[MAX_PATH];
    sprintf_s(outDir, "%s%s", drive, dir);

    s_pathHistory[historyKey] = outDir;

    return DialogResult::OK;
}


// [ファイルを保存]ダイアログボックスを表示
DialogResult Dialog::SaveFileName(char* filepath, int size, const char* filter, const char* title, const char* ext, HWND hWnd)
{
    // 履歴を分けるためのキー
    std::string historyKey = title ? title : "Default";

    // 初期パス設定
    char dirname[MAX_PATH];
    if (filepath[0] != '\0') // ★バグ修正: '\0'
    {
        char drive[MAX_PATH];
        char dir[MAX_PATH];
        ::_splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);
        sprintf_s(dirname, "%s%s", drive, dir);
    }
    else
    {
        dirname[0] = '\0';
    }

    // 履歴からの復元
    if (dirname[0] == '\0')
    {
        if (s_pathHistory.find(historyKey) != s_pathHistory.end())
        {
            strcpy_s(dirname, MAX_PATH, s_pathHistory[historyKey].c_str());
        }
    }

    for (char* p = dirname; *p != '\0'; p++)
    {
        if (*p == '/')
            *p = '\\';
    }

    if (filter == nullptr)
    {
        filter = "All Files\0*.*\0\0";
    }

    // 構造体セット
    OPENFILENAMEA   ofn;
    memset(&ofn, 0, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = filepath;
    ofn.nMaxFile = size;
    ofn.lpstrTitle = title;
    ofn.lpstrInitialDir = (dirname[0] != '\0') ? dirname : nullptr;
    ofn.lpstrDefExt = ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    // カレントディレクトリ取得
    char current_dir[MAX_PATH];
    if (!::GetCurrentDirectoryA(MAX_PATH, current_dir))
    {
        current_dir[0] = '\0';
    }

    // ダイアログオープン
    if (::GetSaveFileNameA(&ofn) == FALSE)
    {
        return DialogResult::Cancel;
    }

    // カレントディレクトリ復帰
    if (current_dir[0] != '\0')
    {
        ::SetCurrentDirectoryA(current_dir);
    }

    // ★修正: 履歴の保存
    char drive[MAX_PATH];
    char dir[MAX_PATH];
    ::_splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);

    char outDir[MAX_PATH];
    sprintf_s(outDir, "%s%s", drive, dir);

    s_pathHistory[historyKey] = outDir;

    return DialogResult::OK;
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