#include "LuaScriptSystem.h"
#include "ECS/Core/CCL_World.h"

// 作成したバインダーをインクルード
#include "Bind/LuaBind_Entity.h"
#include "Bind/LuaBind_Scene.h"
#include "Bind/LuaBind_Components.h"

#include <fstream> // ファイル読み込み用
#include <iostream>
#include <sstream>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// Windows API (OutputDebugStringA用)
#ifdef _WIN32
#include <windows.h>
#endif

LuaScriptSystem::LuaScriptSystem() : IfSystem("LuaScriptSystem") {}

LuaScriptSystem::~LuaScriptSystem()
{
    // Lua本体が死ぬ前にデータを片付ける
    _scriptDataMap.clear();
    _scriptCache.clear();
}

void LuaScriptSystem::LogToDebug(const std::string &msg)
{
#ifdef _WIN32
    std::string out = "[LuaSystem] " + msg + "\n";
    OutputDebugStringA(out.c_str());
#else
    std::cout << "[LuaSystem] " << msg << std::endl;
#endif
}

void LuaScriptSystem::Initialize()
{
    LogToDebug("Initializing...");

    // 1. 基本ライブラリのロード
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // 2. ログ関数の登録
    lua.set_function("CppLog", [this](std::string msg) { LogToDebug("[Script] " + msg); });

    // 3. ★バインダーを使って機能を登録
    //    ここがエントリーポイントになります
    LuaBind_Entity::Bind(lua, _world);
    LuaBind_Scene::Bind(lua, _world);
    LuaBind_Components::Bind(lua, _world);

    // 他のバインダーもここで呼ぶ
    // LuaBind_Input::Bind(lua);
    // LuaBind_Audio::Bind(lua);

    LogToDebug("Initialized and Bindings Registered.");
}

void LuaScriptSystem::Update(float dt)
{
    // メインスレッドで安全に実行
    auto view = _world->View<LuaScriptComponent>();

    for (auto id : view) {
        auto *script = _world->GetComponent<LuaScriptComponent>(id);
        if (!script) continue;

        // 初回ロード
        if (!script->isLoaded) {
            LoadScript(id, script->scriptPath);
            script->isLoaded = true;
        }

        // 実行
        auto it = _scriptDataMap.find(id);
        if (it != _scriptDataMap.end()) {
            auto &data = it->second;

            // 関数が有効なら実行
            if (data.updateFunc.valid()) {
                // 引数: dt, id (環境は既に持っているのでself不要)
                auto result = data.updateFunc(dt, id);

                if (!result.valid()) {
                    sol::error err = result;
                    LogToDebug("Runtime Error (Entity " + std::to_string(id) + "): " + err.what());

                    // ★追加: エラーが出た関数は無効化してログスパムを防ぐ
                    data.updateFunc = sol::nil;
                }
            }
        }
    }
}

void LuaScriptSystem::OnEntityDestroyed(CCL::ECS::EntityID id) { _scriptDataMap.erase(id); }

// ファイル読み込みヘルパー
bool LuaScriptSystem::ReadFile(const std::string &path, std::string &outContent)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    outContent = buffer.str();
    return true;
}

void LuaScriptSystem::LoadScript(CCL::ECS::EntityID id, const std::string &path)
{

    // --- (毎回必ずディスクから最新を読み込む！) ---
    std::string content;

    // ファイル読み込みを試みる
    if (ReadFile(path, content)) {
        // キャッシュを最新の内容で上書き更新する
        _scriptCache[path] = content;
        LogToDebug("Reloaded Script: " + path);
    }
    else {
        // 読み込み失敗時は、もしキャッシュがあればそれを使う（保険）
        if (_scriptCache.find(path) == _scriptCache.end()) {
            LogToDebug("Failed to load script: " + path);
            return;
        }
    }

    // 2. 環境（サンドボックス）の作成
    sol::environment env(lua, sol::create, lua.globals());
    env["entity_id"] = id;

    // 3. キャッシュされた文字列コードからスクリプトを実行
    //    (script_file ではなく script を使う)
    const std::string &code   = _scriptCache[path];
    auto               result = lua.script(code, env, sol::script_pass_on_error);

    if (!result.valid()) {
        sol::error err = result;
        LogToDebug("Syntax Error in " + path + ": " + err.what());
        return;
    }

    // 4. データ構築
    ScriptData data;
    data.env        = env;
    data.updateFunc = env["Update"]; // 環境内から関数を取得

    if (!data.updateFunc.valid()) {
        LogToDebug("Warning: No 'Update' function in " + path);
    }

    _scriptDataMap[id] = std::move(data);
}

REGISTER_LOGIC_SYSTEM(LuaScriptSystem, Priority::LogicStage::L02_Update);