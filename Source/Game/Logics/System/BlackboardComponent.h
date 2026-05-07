#pragma once
#include <string>
#include <unordered_map>
#include "Engine/Core/Math/StringHash.h"

// アニメーション以外（AIやアクション制御）が参照する共有メモリ
struct BlackboardComponent
{
    std::unordered_map<uint32_t, bool> bools;
    std::unordered_map<uint32_t, std::string> strings;

    void SetBool(const std::string& key, bool val) {
        bools[CCL::Utils::HashString(key.c_str())] = val;
    }

    void SetString(const std::string& key, const std::string& val) {
        strings[CCL::Utils::HashString(key.c_str())] = val;
    }

    // 他のシステムが「今、無敵状態か？」などを聞くときに使う
    bool GetBool(const char* key) const {
        auto it = bools.find(CCL::Utils::HashString(key));
        return (it != bools.end()) ? it->second : false;
    }
};
