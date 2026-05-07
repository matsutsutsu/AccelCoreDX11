#include "Dialog.h"
#include <cstdio>
#include <fstream>
#include <json.hpp>

// =============================================================================
// 内部ユーティリティ：Shift-JIS ↔ UTF-8 変換
// =============================================================================
namespace
{
    std::string SjisToUtf8(const std::string& sjis)
    {
        if (sjis.empty()) return "";
        int wlen = MultiByteToWideChar(CP_ACP, 0, sjis.c_str(), -1, nullptr, 0);
        std::wstring wstr(wlen, 0);
        MultiByteToWideChar(CP_ACP, 0, sjis.c_str(), -1, &wstr[0], wlen);

        int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(u8len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], u8len, nullptr, nullptr);
        utf8.resize(strlen(utf8.c_str()));
        return utf8;
    }

    std::string Utf8ToSjis(const std::string& utf8)
    {
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

    // -------------------------------------------------------------------------
    // 選択されたファイルパスからディレクトリ部分だけを取り出す
    // -------------------------------------------------------------------------
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
        return "Default";
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

    // ---- 2D アセット ----
    const DialogConfig Texture = {
        /* title      */ "テクスチャファイルを選択",
        /* filter     */ "画像ファイル\0*.png;*.jpg;*.jpeg;*.tga;*.dds;*.bmp\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0TGA\0*.tga\0DDS\0*.dds\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Textures",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Texture",
    };

    const DialogConfig Sprite = {
        /* title      */ "スプライト画像を選択",
        /* filter     */ "スプライト画像\0*.png;*.jpg;*.jpeg\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0すべてのファイル\0*.*\0",
        /* defaultDir */ "Assets/Sprites",
        /* ext        */ nullptr,
        /* multiSelect*/ false,
        /* historyKey */ "Sprite",
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
        /* defaultDir */ "Assets/Scenes",
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


// =============================================================================
// Dialog::OpenFileName（DialogConfig 版）
// =============================================================================
DialogResult Dialog::OpenFileName(char* filepath, int size, const DialogConfig& config, HWND hWnd)
{
    const std::string historyKey = ResolveHistoryKey(config);
    const char* initDir = ResolveInitialDir(historyKey, config.defaultDir);

    OPENFILENAMEA ofn;
    InitOFN(ofn, filepath, size, config, hWnd);
    ofn.lpstrInitialDir = initDir;
    ofn.Flags |= OFN_FILEMUSTEXIST;

    if (config.multiSelect)
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameA(&ofn))
    {
        s_history[historyKey] = ExtractDirectory(filepath);
        return DialogResult::OK;
    }
    return DialogResult::Cancel;
}


// =============================================================================
// Dialog::SaveFileName（DialogConfig 版）
// =============================================================================
DialogResult Dialog::SaveFileName(char* filepath, int size, const DialogConfig& config, HWND hWnd)
{
    const std::string historyKey = ResolveHistoryKey(config);
    const char* initDir = ResolveInitialDir(historyKey, config.defaultDir);

    OPENFILENAMEA ofn;
    InitOFN(ofn, filepath, size, config, hWnd);
    ofn.lpstrInitialDir = initDir;
    ofn.Flags |= OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn))
    {
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
// 後方互換 API（既存コードをそのまま動かしたい場合）
//   ※ initialDir が非 nullptr のときは履歴より優先します。
// =============================================================================
DialogResult Dialog::OpenFileName(char* filepath, int size,
    const char* filter, const char* title,
    const char* initialDir, HWND hWnd, bool multiSelect)
{
    DialogConfig cfg;
    cfg.title = title;
    cfg.filter = filter;
    cfg.multiSelect = multiSelect;
    // historyKey は title から自動解決

    const std::string historyKey = ResolveHistoryKey(cfg);

    OPENFILENAMEA ofn;
    InitOFN(ofn, filepath, size, cfg, hWnd);
    ofn.Flags |= OFN_FILEMUSTEXIST;
    if (multiSelect)
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // initialDir が明示指定されていればそれを最優先、なければ履歴
    if (initialDir != nullptr)
        ofn.lpstrInitialDir = initialDir;
    else
        ofn.lpstrInitialDir = ResolveInitialDir(historyKey, nullptr);

    if (GetOpenFileNameA(&ofn))
    {
        s_history[historyKey] = ExtractDirectory(filepath);
        return DialogResult::OK;
    }
    return DialogResult::Cancel;
}

DialogResult Dialog::SaveFileName(char* filepath, int size,
    const char* filter, const char* title,
    const char* initialDir, const char* ext, HWND hWnd)
{
    DialogConfig cfg;
    cfg.title = title;
    cfg.filter = filter;
    cfg.ext = ext;

    const std::string historyKey = ResolveHistoryKey(cfg);

    OPENFILENAMEA ofn;
    InitOFN(ofn, filepath, size, cfg, hWnd);
    ofn.Flags |= OFN_OVERWRITEPROMPT;

    if (initialDir != nullptr)
        ofn.lpstrInitialDir = initialDir;
    else
        ofn.lpstrInitialDir = ResolveInitialDir(historyKey, nullptr);

    if (GetSaveFileNameA(&ofn))
    {
        s_history[historyKey] = ExtractDirectory(filepath);
        return DialogResult::OK;
    }
    return DialogResult::Cancel;
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
// 履歴の読み込み / 書き込み
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
                // JSON は UTF-8、内部では Shift-JIS で保持
                s_history[item.key()] = Utf8ToSjis(item.value().get<std::string>());
            }
        }
    }
    catch (...)
    {
        // ファイルが壊れていても無視してデフォルト起動
    }
}

void Dialog::SaveHistory(const char* settingsFilePath)
{
    nlohmann::json j;

    // 既存ファイルを読み込んで追記（他のエディタ設定を上書きしない）
    std::ifstream inFile(settingsFilePath);
    if (inFile.is_open())
    {
        try { inFile >> j; }
        catch (...) {}
        inFile.close();
    }

    // Shift-JIS → UTF-8 に変換して JSON に書き込む
    for (const auto& [key, path] : s_history)
    {
        j["DialogHistory"][key] = SjisToUtf8(path);
    }

    std::ofstream outFile(settingsFilePath);
    if (outFile.is_open())
    {
        outFile << std::setw(4) << j << std::endl;
    }
}