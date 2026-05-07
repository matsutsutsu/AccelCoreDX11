#include "Dialog.h"
#include <cstdio>
#include <fstream>
#include <json.hpp>

// =============================================================================
// 内部ユーティリティ：UTF-8 ↔ UTF-16 (WideChar) 変換
// モダンWindows開発では、内部文字列(UTF-8)をUnicode API(W版)に渡すのが標準です。
// =============================================================================
namespace
{
    // 通常の文字列を UTF-16 に変換
    std::wstring Utf8ToWString(const char* utf8)
    {
        if (!utf8 || utf8[0] == '\0') return L"";
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        std::wstring wstr(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wstr[0], wlen);
        return wstr;
    }

    // フィルタ文字列専用の変換（途中の \0 で切れないようにダブルヌル \0\0 まで読み取る）
    std::wstring Utf8ToWStringFilter(const char* utf8Filter)
    {
        if (!utf8Filter) return L"";
        size_t len = 0;
        // 連続する \0\0 を探す
        while (!(utf8Filter[len] == '\0' && utf8Filter[len + 1] == '\0')) {
            len++;
        }
        len += 2; // 末尾の \0\0 も含める

        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Filter, (int)len, nullptr, 0);
        std::wstring wstr(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Filter, (int)len, &wstr[0], wlen);
        return wstr;
    }

    // UTF-16 から UTF-8 に変換
    std::string WStringToUtf8(const wchar_t* wstr)
    {
        if (!wstr || wstr[0] == L'\0') return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        std::string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], len, nullptr, nullptr);
        str.resize(strlen(str.c_str()));
        return str;
    }

    // 選択されたファイルパスからディレクトリ部分だけを取り出す
    std::string ExtractDirectory(const char* filepath)
    {
        char drive[MAX_PATH], dir[MAX_PATH];
        _splitpath_s(filepath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);
        char outDir[MAX_PATH];
        sprintf_s(outDir, "%s%s", drive, dir);
        return outDir;
    }
} // anonymous namespace


// =============================================================================
// モジュール内部の状態（ファイルスコープ）
// =============================================================================
namespace
{
    // キー → 最後に開いたフォルダ（Shift-JIS で保持）
    std::unordered_map<std::string, std::string> s_history;

    // キー → プリセットのデフォルトフォルダ（実行時に SetPresetDefaultDir で変更可能）
    std::unordered_map<std::string, std::string> s_presetDefaultDirs;

    // キー → 登録済みカスタムプリセット
    std::unordered_map<std::string, DialogConfig> s_registeredPresets;

    // -------------------------------------------------------------------------
    // 有効なヒストリーキーを決定する（空なら title を使用）
    // -------------------------------------------------------------------------
    std::string ResolveHistoryKey(const DialogConfig& config)
    {
        if (!config.historyKey.empty())
            return config.historyKey;
        if (config.title != nullptr && config.title[0] != '\0')
            return config.title;

        // 【設計の要】キーが指定されていない設定を通さない
        assert(false && "DialogConfig MUST have a valid historyKey or title.");
        return "Unknown";
    }

    // -------------------------------------------------------------------------
    // 初期フォルダを決定する
    //
    // 優先順位:
    //   [1] 履歴がある           → 前回ユーザーが開いたフォルダ
    //   [2] defaultDir がある    → プリセット / SetPresetDefaultDir の値
    //   [3] 何もない             → OS 任せ（カレントディレクトリ等）
    //
    // ※ 旧 API の initialDir は呼び出し側で lpstrInitialDir に直接セットします。
    // -------------------------------------------------------------------------
    const char* ResolveInitialDir(const std::string& historyKey, const char* configDefaultDir)
    {
        // [1] 履歴優先
        auto histIt = s_history.find(historyKey);
        if (histIt != s_history.end() && !histIt->second.empty())
            return histIt->second.c_str();

        // [2] 実行時に上書きされたデフォルトフォルダ
        auto dirIt = s_presetDefaultDirs.find(historyKey);
        if (dirIt != s_presetDefaultDirs.end() && !dirIt->second.empty())
            return dirIt->second.c_str();

        // [3] DialogConfig に直接書かれた defaultDir
        if (configDefaultDir != nullptr && configDefaultDir[0] != '\0')
            return configDefaultDir;

        return nullptr; // OS 任せ
    }

    // -------------------------------------------------------------------------
    // 共通の OPENFILENAMEA 初期化
    // -------------------------------------------------------------------------
    void InitOFN(OPENFILENAMEA& ofn, char* filepath, int size, const DialogConfig& config, HWND hWnd)
    {
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFile = filepath;
        ofn.nMaxFile = size;
        ofn.lpstrFilter = config.filter;
        ofn.lpstrTitle = config.title;
        ofn.lpstrDefExt = config.ext;
        ofn.nFilterIndex = 1;
        // OFN_NOCHANGEDIR: ダイアログ後にカレントディレクトリが変わるのを防ぐ（必須）
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    }
} // anonymous namespace


// =============================================================================
// DialogPreset の定義
//
// defaultDir はプロジェクトの構造に合わせて変更してください。
// 実行時に Dialog::SetPresetDefaultDir("Model", "your/path") で上書きできます。
// =============================================================================
namespace DialogPreset
{
    // ---- 3D アセット ----
    const DialogConfig Model = {
        /* title      */ "3D モデルファイルを選択",
        /* filter     */ "3Dモデル\0*.fbx;*.obj;*.gltf;*.glb\0FBX\0*.fbx\0OBJ\0*.obj\0GLTF/GLB\0*.gltf;*.glb\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Models",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Model",
    };

    const DialogConfig Animation = {
        /* title      */ "アニメーションファイルを選択",
        /* filter     */ "アニメーション\0*.fbx;*.bvh\0FBX\0*.fbx\0BVH\0*.bvh\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Animations",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Animation",
    };

    const DialogConfig BehaviorTree = {
        "AI ビヘイビアツリーを選択",
        "BehaviorTree\0*.json\0すべてのファイル\0*.*\0",
        "Assets/BehaviorTree",
        "json",
        false,
        "BehaviorTree",
    };

    // アニメーション：カーブデータ（TestCurve_01.json 等 [cite: 2]）
    const DialogConfig AnimCurve = {
        "アニメーションカーブを選択",
        "Curve Data\0*.json\0",
        "Assets/Animations/Curve",
        "json",
        false,
        "AnimCurve"
    };

    // アニメーション：グラフノード（Boss_AnimGraphNode.json 等 [cite: 2]）
    const DialogConfig AnimNode = {
        "アニメーショングラフノードを選択",
        "Anim Node\0*.json\0",
        "Assets/Animations/Node",
        "json",
        false,
        "AnimNode"
    };

    // アニメーション：シーケンス（Jammo_Run.json 等 [cite: 3]）
    const DialogConfig AnimSequence = {
        "アニメーションシーケンスを選択",
        "Anim Sequence\0*.json\0",
        "Assets/Animations/Sequence",
        "json",
        false,
        "AnimSequence"
    };

    // テクスチャ：VFX用（VFX_Flash_01.png 等 [cite: 101]）
    const DialogConfig TextureVFX = {
        "VFX用テクスチャを選択",
        "VFX Texture\0*.png;*.jpg;*.tga\0",
        "Assets/Textures/VFX",
        nullptr,
        false,
        "TextureVFX"
    };

    // ---- 3D アセット ----
    const DialogConfig Texture = {
        /* title      */ "テクスチャファイルを選択",
        /* filter     */ "画像ファイル\0*.png;*.jpg;*.jpeg;*.tga;*.dds;*.bmp\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0TGA\0*.tga\0DDS\0*.dds\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Textures",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Texture",
    };

	// ---- 2D アセット ----
    const DialogConfig UI = {
        /* title      */ "スプライト画像を選択",
        /* filter     */ "スプライト画像\0*.png;*.jpg;*.jpeg\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/UI",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "UI",
    };

    // ---- オーディオ ----
    const DialogConfig Audio = {
        /* title      */ "オーディオファイルを選択",
        /* filter     */ "オーディオ\0*.wav;*.mp3;*.ogg\0WAV\0*.wav\0MP3\0*.mp3\0OGG\0*.ogg\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Audio",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Audio",
    };

    const DialogConfig BGM = {
        /* title      */ "BGM ファイルを選択",
        /* filter     */ "BGM\0*.mp3;*.ogg\0MP3\0*.mp3\0OGG\0*.ogg\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Audio/BGM",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "BGM",
    };

    const DialogConfig SE = {
        /* title      */ "効果音ファイルを選択",
        /* filter     */ "効果音\0*.wav;*.ogg\0WAV\0*.wav\0OGG\0*.ogg\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Audio/SE",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "SE",
    };

    // ---- シーン / データ ----
    const DialogConfig Scene = {
        /* title      */ "シーンファイルを選択",
        /* filter     */ "シーン\0*.scene;*.json\0SCENE\0*.scene\0JSON\0*.json\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Scene",
        /* ext        */ "scene",
        /* multiSelect*/ false,
        /* historyKey */ "Scene",
    };

    const DialogConfig Prefab = {
        /* title      */ "プレハブファイルを選択",
        /* filter     */ "プレハブ\0*.prefab;*.json\0PREFAB\0*.prefab\0JSON\0*.json\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Prefabs",
        /* ext        */ "prefab",
        /* multiSelect*/ false,
        /* historyKey */ "Prefab",
    };

    const DialogConfig Config = {
        /* title      */ "設定ファイルを選択",
        /* filter     */ "設定ファイル\0*.json;*.ini;*.xml\0JSON\0*.json\0INI\0*.ini\0XML\0*.xml\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Config",
        /* ext        */ "json",
        /* multiSelect*/ false,
        /* historyKey */ "Config",
    };

    // ---- コード / シェーダー ----
    const DialogConfig Script = {
        /* title      */ "スクリプトファイルを選択",
        /* filter     */ "C++ファイル\0*.cpp;*.h;*.hpp\0CPP\0*.cpp\0Header\0*.h;*.hpp\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Source",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Script",
    };

    const DialogConfig Shader = {
        /* title      */ "シェーダーファイルを選択",
        /* filter     */ "シェーダー\0*.hlsl;*.glsl;*.fx;*.vert;*.frag\0HLSL\0*.hlsl\0GLSL\0*.glsl\0FX\0*.fx\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Shaders",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Shader",
    };

    // ---- 汎用 ----
    const DialogConfig Any = {
        /* title      */ "ファイルを選択",
        /* filter     */ "すべてのファイル\0*.*\0",
        /* defaultDir */ nullptr,
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Any",
    };
} // namespace DialogPreset


DialogResult Dialog::OpenFileName(char* filepath, int size, const DialogConfig& config, HWND hWnd)
{
    const std::string historyKey = ResolveHistoryKey(config);
    const char* initDir = ResolveInitialDir(historyKey, config.defaultDir);

    // UTF-8 の設定値を UTF-16 (WideChar) に変換
    std::wstring wTitle = Utf8ToWString(config.title);
    std::wstring wFilter = Utf8ToWStringFilter(config.filter);
    std::wstring wInitDir = Utf8ToWString(initDir);
    std::wstring wDefExt = Utf8ToWString(config.ext);

    wchar_t wFilepath[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = wFilepath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = wFilter.empty() ? nullptr : wFilter.c_str();
    ofn.lpstrTitle = wTitle.empty() ? nullptr : wTitle.c_str();
    ofn.lpstrDefExt = wDefExt.empty() ? nullptr : wDefExt.c_str();
    ofn.lpstrInitialDir = wInitDir.empty() ? nullptr : wInitDir.c_str();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

    if (config.multiSelect)
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // Unicode版 API (GetOpenFileNameW) を呼び出し
    if (GetOpenFileNameW(&ofn))
    {
        // 取得したUTF-16パスをUTF-8に変換して出力バッファに書き込む
        std::string resultUtf8 = WStringToUtf8(wFilepath);
        strcpy_s(filepath, size, resultUtf8.c_str());

        s_history[historyKey] = ExtractDirectory(filepath);
        return DialogResult::OK;
    }
    return DialogResult::Cancel;
}

DialogResult Dialog::SaveFileName(char* filepath, int size, const DialogConfig& config, HWND hWnd)
{
    const std::string historyKey = ResolveHistoryKey(config);
    const char* initDir = ResolveInitialDir(historyKey, config.defaultDir);

    std::wstring wTitle = Utf8ToWString(config.title);
    std::wstring wFilter = Utf8ToWStringFilter(config.filter);
    std::wstring wInitDir = Utf8ToWString(initDir);
    std::wstring wDefExt = Utf8ToWString(config.ext);

    wchar_t wFilepath[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = wFilepath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = wFilter.empty() ? nullptr : wFilter.c_str();
    ofn.lpstrTitle = wTitle.empty() ? nullptr : wTitle.c_str();
    ofn.lpstrDefExt = wDefExt.empty() ? nullptr : wDefExt.c_str();
    ofn.lpstrInitialDir = wInitDir.empty() ? nullptr : wInitDir.c_str();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn))
    {
        std::string resultUtf8 = WStringToUtf8(wFilepath);
        strcpy_s(filepath, size, resultUtf8.c_str());

        s_history[historyKey] = ExtractDirectory(filepath);
        return DialogResult::OK;
    }
    return DialogResult::Cancel;
}


// =============================================================================
// カスタムプリセットの登録 / キー指定で呼び出す API
// =============================================================================
void Dialog::Register(const std::string& key, const DialogConfig& config)
{
    s_registeredPresets[key] = config;
    // historyKey が空なら登録キーをそのまま使う
    if (s_registeredPresets[key].historyKey.empty())
        s_registeredPresets[key].historyKey = key;
}

DialogResult Dialog::OpenByKey(char* filepath, int size, const std::string& key, HWND hWnd)
{
    auto it = s_registeredPresets.find(key);
    if (it == s_registeredPresets.end())
        return Dialog::OpenFileName(filepath, size, DialogPreset::Any, hWnd); // フォールバック

    return Dialog::OpenFileName(filepath, size, it->second, hWnd);
}

DialogResult Dialog::SaveByKey(char* filepath, int size, const std::string& key, HWND hWnd)
{
    auto it = s_registeredPresets.find(key);
    if (it == s_registeredPresets.end())
        return Dialog::SaveFileName(filepath, size, DialogPreset::Any, hWnd); // フォールバック

    return Dialog::SaveFileName(filepath, size, it->second, hWnd);
}



// =============================================================================
// ユーティリティ
// =============================================================================
void Dialog::SetPresetDefaultDir(const std::string& historyKey, const std::string& dir)
{
    s_presetDefaultDirs[historyKey] = dir;
}

void Dialog::ClearHistory(const std::string& historyKey)
{
    s_history.erase(historyKey);
}

void Dialog::ClearAllHistory()
{
    s_history.clear();
}


// =============================================================================
// 履歴の読み込み / 書き込み (UTF-8 直結版)
// =============================================================================
void Dialog::LoadHistory(const char* settingsFilePath)
{
    std::ifstream file(settingsFilePath);
    if (!file.is_open()) return;

    try
    {
        nlohmann::json j;
        file >> j;
        if (j.contains("DialogHistory"))
        {
            for (auto& item : j["DialogHistory"].items())
            {
                // 変更点: Shift-JIS変換を挟まず、UTF-8のままメモリ(s_history)に格納する
                s_history[item.key()] = item.value().get<std::string>();
            }
        }
    }
    catch (...)
    {
        // パース失敗時は履歴を無視して継続
    }
}

void Dialog::SaveHistory(const char* settingsFilePath)
{
    nlohmann::json j;

    // 既存のJSONを読み込んで保持する（他のエディタ設定を消さないため）
    std::ifstream inFile(settingsFilePath);
    if (inFile.is_open())
    {
        try { inFile >> j; }
        catch (...) {}
        inFile.close();
    }

    // 変更点: Shift-JIS変換を挟まず、UTF-8のままJSONへ書き出す
    for (const auto& [key, path] : s_history)
    {
        j["DialogHistory"][key] = path;
    }

    std::ofstream outFile(settingsFilePath);
    if (outFile.is_open())
    {
        outFile << std::setw(4) << j << std::endl;
    }
}