#pragma once
#include <vector>
#include <cstdint>

// ===================================================================================
// ファイル: AnimParametersComponent.h
// 概要: アニメーション状態遷移のための「パラメータ掲示板」コンポーネント
// 
// [ 役割 ]
// ロジックシステム（移動やAI）が計算した「現在の状態（速度、接地判定など）」を書き込み、
// AnimControllerSystem（評価器）がそれを読み取るためのデータコンテナ。
// std::unordered_map を避け、フラットな vector の線形探索を用いることで、
// データ指向設計（DOD）に基づいた高速なキャッシュヒットを実現している。
// ===================================================================================

// CPUキャッシュラインに収まりやすいフラットなパラメータボード
struct AnimParametersComponent {
    struct FloatParam { uint32_t hash; float value; };
    struct BoolParam { uint32_t hash; bool value; };

    std::vector<FloatParam> floats;
    std::vector<BoolParam>  bools;
    std::vector<uint32_t>   triggers; // 発生したトリガーのIDリスト

    void SetFloat(uint32_t hash, float val) {
        for (auto& p : floats) {
            if (p.hash == hash) { p.value = val; return; }
        }
        floats.push_back({ hash, val });
    }

    float GetFloat(uint32_t hash) const {
        for (const auto& p : floats) {
            if (p.hash == hash) return p.value;
        }
        return 0.0f; // デフォルト値
    }

    void SetBool(uint32_t hash, bool val) {
        for (auto& p : bools) {
            if (p.hash == hash) { p.value = val; return; }
        }
        bools.push_back({ hash, val });
    }

    bool GetBool(uint32_t hash) const {
        for (const auto& p : bools) {
            if (p.hash == hash) return p.value;
        }
        return false;
    }

    void SetTrigger(uint32_t hash) {
        // 既に存在していなければ追加
        if (std::find(triggers.begin(), triggers.end(), hash) == triggers.end()) {
            triggers.push_back(hash);
        }
    }

    bool HasTrigger(uint32_t hash) const {
        return std::find(triggers.begin(), triggers.end(), hash) != triggers.end();
    }

    void ConsumeTrigger(uint32_t hash) {
        auto it = std::find(triggers.begin(), triggers.end(), hash);
        if (it != triggers.end()) {
            triggers.erase(it);
        }
    }
};
