#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <json.hpp>

// キーフレーム（時間と値のペア）
struct CurveKey {
    float time;  // 0.0 ~ 1.0 (アニメーション進捗率)
    float value; // その時の速度倍率

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CurveKey, time, value)
};

// 実行用カーブデータ
struct AnimationCurve {
    std::string name;
    std::vector<CurveKey> keys;

    // 指定された進捗率(0~1)における値を線形補間で取得
    float Evaluate(float t) const {
        if (keys.empty()) return 1.0f;
        if (t <= keys.front().time) return keys.front().value;
        if (t >= keys.back().time) return keys.back().value;

        // 現在の時間を挟む2つのキーフレームを見つける
        auto it = std::lower_bound(keys.begin(), keys.end(), t,
            [](const CurveKey& k, float targetTime) { return k.time < targetTime; });

        auto k1 = *(it - 1);
        auto k2 = *it;

        // 線形補間 (Lerp) で間を計算する
        float lerpT = (t - k1.time) / (k2.time - k1.time);
        return k1.value + (k2.value - k1.value) * lerpT;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AnimationCurve, name, keys)
};