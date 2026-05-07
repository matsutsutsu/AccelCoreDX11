#pragma once

// デバッグビルド時のみ安全チェックを有効にする（リリース時は高速化のためOFF）
#ifdef _DEBUG
#define SOL_ALL_SAFETIES_ON 1
#endif

#include "ECS/System/CCL_System.h"
#include "LuaScriptComponent.h"

// sol2 と 標準ライブラリ
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

class LuaScriptSystem : public CCL::ECS::IfSystem < LuaScriptSystem,
    CCL::ECS::Write < LuaScriptComponent >> {

    // スクリプトごとの実行環境データ
    struct ScriptData {
        sol::environment        env;        // このエンティティ専用のサンドボックス
        sol::protected_function updateFunc; // Update関数
    };

  private:
    // メンバ変数 (定義順序が重要: luaが最後に破棄されるように)
    sol::state                                         lua;
    std::unordered_map<CCL::ECS::EntityID, ScriptData> _scriptDataMap;

    // ファイル読み込みキャッシュ (パス -> ファイルの中身)
    // これにより、同じスクリプトを使う敵が100体いてもディスク読み込みは1回だけで済む
    std::unordered_map<std::string, std::string> _scriptCache;

    // 内部ヘルパー
    void LogToDebug(const std::string &msg);
    void LoadScript(CCL::ECS::EntityID id, const std::string &path);
    bool ReadFile(const std::string &path, std::string &outContent);

  public:
    LuaScriptSystem();
    virtual ~LuaScriptSystem();

    void Initialize() override;
    void Update(float dt) override;
    void OnEntityDestroyed(CCL::ECS::EntityID id);


    // public部分に追加
    template <typename... Args>
    void CallFunction(CCL::ECS::EntityID id, const std::string &funcName, Args &&...args)
    {
        // 1. そのエンティティがスクリプトを持っているか確認
        auto it = _scriptDataMap.find(id);
        if (it == _scriptDataMap.end()) return;

        // 2. 関数を取得 (例: "OnTriggerEnter")
        auto                   &data = it->second;
        sol::protected_function func = data.env[funcName];

        if (func.valid()) {
            // 3. 実行！
            auto result = func(std::forward<Args>(args)...);
            if (!result.valid()) {
                sol::error err = result;
                LogToDebug("Lua Event Error: " + std::string(err.what()));
            }
        }
    }
};