#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <json.hpp> // cereal を廃止し json.hpp に切り替え
#include "Engine/GamePlay/Animation/Data/AnimSequence.h"
#include "Engine/GamePlay/Animation/Data/AnimationCurve.h"

// 条件演算子
// 例: param > threshold なら op は Greater
// 例: param < threshold なら op は Less
// 例: param == threshold なら op は Equal
// 例: トリガーが引かれたかどうかを判定する場合は op は Trigger (この場合 threshold は無視される)
// 例: アニメーションの再生が終了したかを判定する場合は op は Finished (threshold は無視される)
enum class AnimConditionOp { Equal, Greater, Less, Trigger, Finished }; 

// 遷移条件
struct AnimCondition {
    uint32_t paramHash = 0;

    std::string paramName = "NewParam";     // エディタ表示用の名前 (例: "Speed")

	// 条件演算子（例: param > threshold なら op は Greater）
    AnimConditionOp op = AnimConditionOp::Equal;
    float threshold = 0.0f;
};

// 遷移の判定ロジック (AND / OR)
enum class AnimTransitionLogic { AND, OR };

// 遷移の矢印
struct AnimTransition {
    uint32_t targetStateHash;

    // ブレンドにかける時間 (秒)
    float blendDuration = 0.2f;

    // デフォルトは AND（すべて満たす）
    AnimTransitionLogic logicType = AnimTransitionLogic::AND;

	// その矢印を進むための条件リスト（複数の条件を全て満たす必要がある）
    std::vector<AnimCondition> conditions;
};

// ステート (ノード)
struct AnimState {
    uint32_t stateHash;
    std::string sequenceName; 

    // 実際に再生するシーケンサーのJSONファイルパス (UnityのMotionにあたるもの)
    std::string sequenceFilePath;


    // --- 拡張パラメータ ---
    bool isLoop = true;
    float playbackSpeed = 1.0f;
    std::string speedCurveName; // 空文字ならカーブなし(等速)
    std::string rootMotionCurveName; // 踏み込み距離用

    // エディタ上でのノードの座標
    float nodePosX = 0.0f;
    float nodePosY = 0.0f;

    // ロード時に ResourceManager などから取得したポインタを繋ぐ場所
    const AnimationCurve* speedCurve = nullptr;

    const AnimSequence* sequence = nullptr;

	// そのステートから伸びる遷移のリスト
    std::vector<AnimTransition> transitions;
};

// グラフ全体
struct AnimStateGraph {
    std::vector<AnimState> states;
    uint32_t entryStateHash = 0;

    // Entryノードの座標
    float entryPosX = 0.0f;
    float entryPosY = 0.0f;

    // Any State (特別遷移) のリスト
    // どのステートにいても、ここの条件を満たせば強制遷移する
    std::vector<AnimTransition> anyStateTransitions;

    // エディタ表示用の Any State ノード座標
    float anyStatePosX = 0.0f;
    float anyStatePosY = 0.0f;
};

// =================================================================
// nlohmann/json 用のシリアライズ定義 (手動変換により制御を容易にする)
// =================================================================
using json = nlohmann::json;

// Condition の変換
inline void to_json(json& j, const AnimCondition& c) {
    j = json{
        {"paramHash", c.paramHash},
        {"paramName", c.paramName}, // ★追加: 文字列をJSONへ書き出す
        {"op", (int)c.op},
        {"threshold", c.threshold}
    };
}
inline void from_json(const json& j, AnimCondition& c) {
    j.at("paramHash").get_to(c.paramHash);

    // 古いJSONファイルには "paramName" が無い可能性があるため、
    // contains() で存在チェックをしてから読み込む（後方互換性の担保）
    if (j.contains("paramName")) {
        j.at("paramName").get_to(c.paramName);
    }
    else {
        c.paramName = ""; // 古いデータの場合は空文字にしておく
    }

    j.at("op").get_to((int&)c.op);
    j.at("threshold").get_to(c.threshold);
}

// Transition の変換
inline void to_json(json& j, const AnimTransition& t) {
    j = json{ {"targetStateHash", t.targetStateHash},
            {"blendDuration", t.blendDuration},
            {"logicType", (int)t.logicType},
            {"conditions", t.conditions} };
}
inline void from_json(const json& j, AnimTransition& t) {
    j.at("targetStateHash").get_to(t.targetStateHash);
    // 古いセーブデータとの互換性のため contains を使う
    if (j.contains("blendDuration")) {
        j.at("blendDuration").get_to(t.blendDuration);
    }
    else {
        t.blendDuration = 0.2f; // デフォルト値
    }

    // 古いセーブデータ対応
    if (j.contains("logicType")) j.at("logicType").get_to((int&)t.logicType);
    else t.logicType = AnimTransitionLogic::AND;
    j.at("conditions").get_to(t.conditions);
}

// State の変換 (to_json / from_json に新規パラメータを追加)
inline void to_json(json& j, const AnimState& s) {
    j = json{
        {"stateHash", s.stateHash},
        {"sequenceName", s.sequenceName},
        {"sequenceFilePath", s.sequenceFilePath},
        {"isLoop", s.isLoop},
        {"playbackSpeed", s.playbackSpeed},
        {"speedCurveName", s.speedCurveName},
        {"rootMotionCurveName", s.rootMotionCurveName},   
        {"nodePosX", s.nodePosX}, // ★追加
        {"nodePosY", s.nodePosY}, // ★追加
        {"transitions", s.transitions}
    };
}
inline void from_json(const json& j, AnimState& s) {
    j.at("stateHash").get_to(s.stateHash);
    j.at("sequenceName").get_to(s.sequenceName);
    if (j.contains("sequenceFilePath")) j.at("sequenceFilePath").get_to(s.sequenceFilePath);
    if (j.contains("isLoop")) j.at("isLoop").get_to(s.isLoop);
    if (j.contains("playbackSpeed")) j.at("playbackSpeed").get_to(s.playbackSpeed);
    if (j.contains("speedCurveName")) j.at("speedCurveName").get_to(s.speedCurveName);
    if (j.contains("rootMotionCurveName")) j.at("rootMotionCurveName").get_to(s.rootMotionCurveName);

    // ★追加: 古いJSONでもクラッシュしないように contains でチェック
    if (j.contains("nodePosX")) j.at("nodePosX").get_to(s.nodePosX);
    if (j.contains("nodePosY")) j.at("nodePosY").get_to(s.nodePosY);

    j.at("transitions").get_to(s.transitions);
}

// Graph の変換
inline void to_json(json& j, const AnimStateGraph& g) {
    j = json{
        {"entryStateHash", g.entryStateHash},
        {"entryPosX", g.entryPosX}, 
        {"entryPosY", g.entryPosY}, 
        {"states", g.states},
        {"anyStateTransitions", g.anyStateTransitions},
        {"anyStatePosX", g.anyStatePosX},              
        {"anyStatePosY", g.anyStatePosY}               
    };
}
inline void from_json(const json& j, AnimStateGraph& g) {
    j.at("entryStateHash").get_to(g.entryStateHash);

    if (j.contains("entryPosX")) j.at("entryPosX").get_to(g.entryPosX);
    if (j.contains("entryPosY")) j.at("entryPosY").get_to(g.entryPosY);

    j.at("states").get_to(g.states);

    // 後方互換性を持たせてロード
    if (j.contains("anyStateTransitions")) j.at("anyStateTransitions").get_to(g.anyStateTransitions);
    if (j.contains("anyStatePosX")) j.at("anyStatePosX").get_to(g.anyStatePosX);
    if (j.contains("anyStatePosY")) j.at("anyStatePosY").get_to(g.anyStatePosY);
}

struct AnimStateMachineComponent {
    // エディタで設定したJSONファイルのパスを記憶（シリアライズ対象）
    std::string graphPath;

    // ロードされたグラフデータの実体（ポインタの指す先）
    AnimStateGraph internalGraph;

	// 現在のステートを示すハッシュ値（ノードID）
    uint32_t currentStateHash = 0;
};