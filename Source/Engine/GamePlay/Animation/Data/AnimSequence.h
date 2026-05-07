#pragma once
#include <string>
#include <vector>
#include <json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "Engine/GamePlay/Animation/Data/AnimationCurve.h"

// 1つのイベント通知
struct AnimNotifyEvent {
    float startTime;
    float endTime;
    std::string eventName;
    std::string stringParam;

    // ★追加: 数値パラメータ（ヒットストップ時間やダメージ倍率など汎用的に使える）
    float floatParam1 = 0.0f; // 例：ヒットストップ時間 (0.12 など)
    float floatParam2 = 0.0f; // 例：ヒットストップ中のスロー倍率 (0.0 なら完全停止)

    // A. 既存の Cereal 用シリアライズ (バイナリ保存用)
    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(startTime), CEREAL_NVP(endTime), CEREAL_NVP(eventName), CEREAL_NVP(stringParam),
            CEREAL_NVP(floatParam1), CEREAL_NVP(floatParam2)); 
    }

   
};

// 全エンティティで共有される「アニメーションの台本」
struct AnimSequence {
    std::string sequenceName;
    std::string targetModelPath; // または targetAnimName
    std::string targetAnimName;
    float duration = 1.0f;
    std::vector<AnimNotifyEvent> events;

    // ★統合されたカーブデータ
    AnimationCurve speedCurve;
    AnimationCurve rootMotionCurve;

    AnimSequence() {
        speedCurve.name = "Speed";
        speedCurve.keys.push_back({ 0.0f, 1.0f });
        speedCurve.keys.push_back({ 1.0f, 1.0f });

        rootMotionCurve.name = "RootMotion";
        rootMotionCurve.keys.push_back({ 0.0f, 1.0f });
        rootMotionCurve.keys.push_back({ 1.0f, 1.0f });
    }

    // A. 既存の Cereal 用シリアライズ
    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(sequenceName), CEREAL_NVP(targetModelPath), CEREAL_NVP(targetAnimName), CEREAL_NVP(duration), CEREAL_NVP(events));
        // ※ Cereal側にもカーブを保存したい場合はここに追加します
    }
};

// ★追加: 安全なJSON相互変換関数（構造体の定義のすぐ下に配置）
inline void to_json(nlohmann::json& j, const AnimNotifyEvent& e) {
    j = nlohmann::json{
        {"startTime", e.startTime}, {"endTime", e.endTime},
        {"eventName", e.eventName}, {"stringParam", e.stringParam},
        {"floatParam1", e.floatParam1}, {"floatParam2", e.floatParam2}
    };
}
inline void from_json(const nlohmann::json& j, AnimNotifyEvent& e) {
    if (j.contains("startTime")) j.at("startTime").get_to(e.startTime);
    if (j.contains("endTime")) j.at("endTime").get_to(e.endTime);
    if (j.contains("eventName")) j.at("eventName").get_to(e.eventName);
    if (j.contains("stringParam")) j.at("stringParam").get_to(e.stringParam);
    // 古いデータには存在しない可能性があるので contains で安全にチェックする
    if (j.contains("floatParam1")) j.at("floatParam1").get_to(e.floatParam1);
    if (j.contains("floatParam2")) j.at("floatParam2").get_to(e.floatParam2);
}

// B. 新しい nlohmann/json 用定義 (関数の外に定義)
inline void to_json(nlohmann::json& j, const AnimSequence& s) {
    j = nlohmann::json{
        {"sequenceName", s.sequenceName},
        {"targetModelPath", s.targetModelPath},
        {"targetAnimName", s.targetAnimName},
        {"duration", s.duration},
        {"events", s.events},
        {"speedCurve", s.speedCurve},
        {"rootMotionCurve", s.rootMotionCurve}
    };
}

inline void from_json(const nlohmann::json& j, AnimSequence& s) {
    if (j.contains("sequenceName")) j.at("sequenceName").get_to(s.sequenceName);
    if (j.contains("targetModelPath")) j.at("targetModelPath").get_to(s.targetModelPath);
    if (j.contains("targetAnimName")) j.at("targetAnimName").get_to(s.targetAnimName);
    if (j.contains("duration")) j.at("duration").get_to(s.duration);
    if (j.contains("events")) j.at("events").get_to(s.events);
    if (j.contains("speedCurve")) j.at("speedCurve").get_to(s.speedCurve);
    if (j.contains("rootMotionCurve")) j.at("rootMotionCurve").get_to(s.rootMotionCurve);
}