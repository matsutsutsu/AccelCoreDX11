#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream> // ファイル出力

// ログの発生元カテゴリ（エンジンが拡張されたらここを増やす）
enum class LogCategory {
    Core,       // エンジン基盤（初期化、シーン遷移など）
    ECS,        // エンティティ・コンポーネント・システム
    Editor,     // エディタ操作、Undo/Redo、ツール関連
    Physics,    // Joltなどの物理演算
    Render,     // 描画、シェーダー、ライト
    Audio,      // FMODなどのサウンド
    Game,       // ゲームロジック（プレイヤー、敵AIなど）
	AI,         // AI関連（パスファインディング、ビヘイビアツリーなど）
    Count       // カテゴリの総数（UI作成用ループに使う）
};

// ログの重要度
enum class LogLevel {
    Info,
    Success,
    Warning,
    Error
};

// 1件のログデータ
struct LogMessage {
    LogCategory category;
    LogLevel level;
    std::string file; // 発生元ファイル名
    int line;         // 発生元行番号
    std::string text;
};

// =========================================================
// ログ管理コア (シングルトン)
// =========================================================
class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    // フォーマット付きでログを記録する関数 (printfと同じ使い方)
    void Log(LogCategory category, LogLevel level, const char* file, int line, const char* fmt, ...);

    // GUI描画用に蓄積されたログを取得
    const std::vector<LogMessage>& GetMessages() const { return _messages; }

    // ログの消去
    void Clear();

private:
    Logger();  
    ~Logger(); 

    std::vector<LogMessage> _messages;
    std::mutex _mutex; // 物理スレッドなど別スレッドから呼ばれてもクラッシュしないための鍵
    
    std::ofstream _fileStream; // ログファイルへのストリーム

    const size_t MAX_LOG_COUNT = 1000; // メモリ爆発を防ぐ上限
};



// ==============================================================================
//  Releaseビルド時の自動消去と、ERRORの生存戦略
// ==============================================================================

// _DEBUG は、Visual StudioがDebugビルドの時に自動で定義してくれる隠しフラグです
#ifdef _DEBUG
    // ---------------------------------------------------------
    // 【Debugビルド】全て記録する
    // ---------------------------------------------------------
#define CCL_LOG_INFO(category, ...)    Logger::Instance().Log(category, LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define CCL_LOG_SUCCESS(category, ...) Logger::Instance().Log(category, LogLevel::Success, __FILE__, __LINE__, __VA_ARGS__)
#define CCL_LOG_WARN(category, ...)    Logger::Instance().Log(category, LogLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)

#else
    // ---------------------------------------------------------
    // 【Releaseビルド】INFO と WARN を「無」にする（処理負荷ゼロ）
    // ---------------------------------------------------------
    // ((void)0) はC++における「何もしない安全な構文」です。
    // コンパイラはこれを見ると、処理を丸ごとスキップ（最適化で消去）します。
#define CCL_LOG_INFO(category, ...)    ((void)0)
#define CCL_LOG_SUCCESS(category, ...) ((void)0)
#define CCL_LOG_WARN(category, ...)    ((void)0)

#endif

// ---------------------------------------------------------
// ★ アーキテクトの絶対ルール：ERROR は Release でも消さない！
// ---------------------------------------------------------
// プレイヤーの環境でゲームがクラッシュした際、原因究明の唯一の手がかり（ダイイングメッセージ）
// となるため、ERRORだけはビルド設定に関わらず常にファイルへ書き出します。
#define CCL_LOG_ERROR(category, ...)   Logger::Instance().Log(category, LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)