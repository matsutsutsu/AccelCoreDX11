#include "Logger.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <Windows.h>
#include <filesystem>

// =========================================================
// フルパスからファイル名だけを抽出するヘルパー
// 理由: __FILE__ は "C:\MyGame\Engine\Scene.cpp" のような
// 超長い絶対パスを返すため、UIが見にくくなるのを防ぐ。
// =========================================================
static const char* ExtractFileName(const char* fullPath) {
    const char* file = strrchr(fullPath, '\\');
    if (!file) file = strrchr(fullPath, '/');
    return file ? file + 1 : fullPath;
}

// Enum から文字列に変換するヘルパー関数
static const char* GetCategoryName(LogCategory category) {
    switch (category) {
    case LogCategory::Core:    return "Core";
    case LogCategory::ECS:     return "ECS";
    case LogCategory::Physics: return "Physics";
    case LogCategory::Render:  return "Render";
    case LogCategory::Audio:   return "Audio";
    case LogCategory::Game:    return "Game";
    case LogCategory::AI:      return "AI";
    case LogCategory::Editor:  return "Editor";
    default:                   return "Unknown";
    }
}

// コンストラクタ：エンジンの起動時に1回だけ呼ばれる
Logger::Logger()
{
    // ★ 改良: ログ専用のフォルダパスを定義
    std::string logDir = "Build/Logs";
    std::string logFile = logDir + "/EngineDebug_Log.txt";

    // フォルダが存在しなければ、自動で作成する（超便利）
    std::filesystem::create_directories(logDir);

    // そのフォルダの中にファイルを作る
    _fileStream.open(logFile, std::ios::out | std::ios::trunc);

    if (_fileStream.is_open()) {
        _fileStream << "=== Engine Log Started ===" << std::endl;
    }
}

// デストラクタ
Logger::~Logger()
{
    if (_fileStream.is_open()) {
        _fileStream << "=== Engine Log Closed ===" << std::endl;
        _fileStream.close();
    }
}

void Logger::Log(LogCategory category, LogLevel level, const char* file, int line, const char* fmt, ...)
{
    // 1. フォーマット文字列の展開
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // カテゴリ名をプレフィックスに含める
    const char* shortFile = ExtractFileName(file);
    char prefix[256];
    // 出力例: [Physics] [JoltPhysicsManager.cpp:142]
    snprintf(prefix, sizeof(prefix), "[%s] [%s:%d] ", GetCategoryName(category), shortFile, line);

    // 2. Visual Studioの「出力」ウィンドウに流す
    OutputDebugStringA(prefix);
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");

    // 3. スレッドセーフ区間
    std::lock_guard<std::mutex> lock(_mutex);

    if (_fileStream.is_open()) {
        const char* levelStr = (level == LogLevel::Error) ? "[ERROR] " :
            (level == LogLevel::Warning) ? "[WARN] " :
            (level == LogLevel::Success) ? "[SUCCESS] " : "[INFO] "; 

        _fileStream << levelStr << prefix << buffer << "\n";

        if (level == LogLevel::Error) {
            _fileStream.flush();
        }
    }

    // category をコンストラクタに追加
    _messages.push_back({ category, level, std::string(shortFile), line, std::string(buffer) });

    if (_messages.size() > MAX_LOG_COUNT) {
        _messages.erase(_messages.begin());
    }
}

void Logger::Clear()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _messages.clear();
}