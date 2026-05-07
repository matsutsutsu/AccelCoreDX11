#pragma once
#include <string>

struct LuaScriptComponent {
    // スクリプトファイル名 (例: "Assets/Script/BossAI.lua")
    std::string scriptPath;

    // スクリプトがロード済みかどうかのフラグ
    bool isLoaded = false;

};