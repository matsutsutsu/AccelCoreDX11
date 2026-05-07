#pragma once
#include "ECS/Common/CCL_Common.h"
#include <string>

// プーリング対象であることを示すタグコンポーネント
struct PooledParticleComponent {
    // どの種類のパーティクルか識別するためのキー (JSONパス)
    char poolKey[128] = {};

    // プール待機中かどうか (trueなら更新・描画しないように制御可)
    bool isSleeping = false;

    void SetKey(const std::string &key) { strcpy_s(poolKey, sizeof(poolKey), key.c_str()); }
};