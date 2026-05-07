#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>

// =============================================================================
// ダイアログの結果
// =============================================================================
enum class DialogResult
{
    Yes,
    No,
    OK,
    Cancel
};

// =============================================================================
// ダイアログの設定構造体
//
// 使い方:
//   DialogConfig cfg;
//   cfg.title      = "モデルファイルを選択";
//   cfg.filter     = "3Dモデル\0*.fbx;*.obj\0";
//   cfg.defaultDir = "Assets/Models";     ← 初回はここから開く
//   cfg.historyKey = "Model";             ← 2回目以降は前回フォルダを記憶
//   Dialog::OpenFileName(buf, sizeof(buf), cfg);
//
// ※ historyKey を省略すると title の値がキーとして使われます。
// ※ defaultDir を省略すると履歴フォルダ → カレントディレクトリの順で使われます。
// =============================================================================
struct DialogConfig
{
    const char* title = nullptr;  // ウィンドウタイトル
    const char* filter = nullptr;  // ファイルフィルタ文字列 ("表示名\0*.ext\0")
    const char* defaultDir = nullptr;  // プリセットの初期フォルダ（履歴より優先される※後述）
    const char* ext = nullptr;  // 保存ダイアログ用デフォルト拡張子 (例: "fbx")
    bool        multiSelect = false;    // 複数選択を許可（OpenFileNameのみ有効）
    std::string historyKey;             // 履歴を識別するキー（省略時は title を使用）

    // ----- 初期フォルダの優先順位 -----
    // [1] 履歴が存在する          → 履歴のフォルダを使用  （ユーザー操作を最優先）
    // [2] defaultDir が設定済み   → defaultDir を使用     （プリセット指定）
    // [3] どちらも無し            → OS のデフォルト（カレントディレクトリ等）
    //
    // ※ initialDir を明示的に渡す OpenFileName / SaveFileName の旧 API では
    //   initialDir が最優先になります（プログラム側で動的に決めたい場合向け）。
};


// =============================================================================
// ゲーム開発でよく使うプリセット集
//
// 使い方:
//   Dialog::OpenFileName(buf, sizeof(buf), DialogPreset::Model);
//
// defaultDir はプロジェクト構成に合わせて変更してください。
// Dialog::SetPresetDefaultDir() で実行時に上書きすることもできます。
// =============================================================================
namespace DialogPreset
{
    // ---- 3D アセット ----
    extern const DialogConfig Model;        // FBX / OBJ / GLTF / GLB
    extern const DialogConfig Animation;    // FBX / BVH (アニメーション専用)

    extern const DialogConfig BehaviorTree;
    extern const DialogConfig AnimCurve;
    extern const DialogConfig AnimNode;
    extern const DialogConfig AnimSequence;
    extern const DialogConfig TextureVFX;

    // ---- 2D アセット ----
    extern const DialogConfig Texture;      // PNG / JPG / TGA / DDS / BMP
    extern const DialogConfig Sprite;       // PNG / JPG (スプライト専用)

    // ---- オーディオ ----
    extern const DialogConfig Audio;        // WAV / MP3 / OGG
    extern const DialogConfig BGM;          // MP3 / OGG (BGM 専用)
    extern const DialogConfig SE;           // WAV / OGG (効果音専用)

    // ---- シーン / データ ----
    extern const DialogConfig Scene;        // .scene / JSON
    extern const DialogConfig Prefab;       // .prefab / JSON
    extern const DialogConfig Config;       // JSON / INI / XML

    // ---- コード / シェーダー ----
    extern const DialogConfig Script;       // CPP / H
    extern const DialogConfig Shader;       // HLSL / GLSL / FX

    // ---- 汎用 ----
    extern const DialogConfig Any;          // すべてのファイル
}


// =============================================================================
// Dialog クラス
// =============================================================================
class Dialog
{
public:
    // =========================================================================
    // ▼ 推奨 API（プリセット / DialogConfig を使う版）
    // =========================================================================

    // [ファイルを開く] DialogConfig 版
    static DialogResult OpenFileName(
        char* filepath,
        int                size,
        const DialogConfig& config,
        HWND               hWnd = NULL);

    // [ファイルを保存] DialogConfig 版
    static DialogResult SaveFileName(
        char* filepath,
        int                size,
        const DialogConfig& config,
        HWND               hWnd = NULL);


    // =========================================================================
    // ▼ キー文字列で登録済みプリセットを呼び出す API
    //
    //   Dialog::Register("MyMap", cfg);
    //   Dialog::OpenByKey(buf, sizeof(buf), "MyMap");
    // =========================================================================
    static void        Register(const std::string& key, const DialogConfig& config);
    static DialogResult OpenByKey(char* filepath, int size, const std::string& key, HWND hWnd = NULL);
    static DialogResult SaveByKey(char* filepath, int size, const std::string& key, HWND hWnd = NULL);


 

    // =========================================================================
    // ▼ ユーティリティ
    // =========================================================================

    // 特定キーのデフォルトフォルダを実行時に変更する
    // （例: プロジェクトを開いた後にプロジェクトルートを反映したい場合）
    static void SetPresetDefaultDir(const std::string& historyKey, const std::string& dir);

    // 特定キーの履歴をクリアする（次回 defaultDir から開き直したい場合）
    static void ClearHistory(const std::string& historyKey);

    // 全履歴をクリアする
    static void ClearAllHistory();

    // 履歴の読み込み / 書き込み
    static void LoadHistory(const char* settingsFilePath = "Build/EditorPrefs.json");
    static void SaveHistory(const char* settingsFilePath = "Build/EditorPrefs.json");
};