#pragma once
#include <string>
#include <vector>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "ECS/Common/CCL_Common.h"

// ===================================================================================
// ファイル: AnimSequence.h
// 概要: 1つのアニメーションの「台本」データ
// 
// [ 役割 ]
// どのアニメーションを再生するか（対象モデル、アニメ名、長さ）の情報に加え、
// 「〇〇秒の時点で足音を鳴らす」「攻撃判定を出す」といった
// アニメーションイベント（AnimNotifyEvent）のリストを保持する。
// マスターデータとしてメモリ上にロードされ、共有される。
// ===================================================================================

// 1つのイベント通知 (エンジンが読み取る点)
struct AnimNotifyEvent {
    float startTime;         // 発火開始時間（秒）
    float endTime;           // 発火終了時間（秒） - 点イベントの場合は startTime と同じ値にする
    std::string eventName;   // "HitBox_Start", "HitBox_End", "Play_Sound" など
    std::string stringParam; // "Assets/Prefabs/Effects/SwordSlash.json" などのパスを書く欄

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(startTime), CEREAL_NVP(endTime), CEREAL_NVP(eventName), CEREAL_NVP(stringParam));
    }
};

// イベントを他システムへ伝えるための手紙
struct AnimNotifyMessage {
    CCL::ECS::EntityID entity;
    std::string eventName;
    std::string stringParam; // 他システムへパラメータ（パスや部位名）を渡すための変数
};


// 全エンティティで共有される「アニメーションの台本」
struct AnimSequence {
    std::string sequenceName;       // 例: "Goblin_Attack"
    std::string targetModelPath;    // 例: "Assets/Models/Goblin.gltf"
    std::string targetAnimName;     // 例: "Attack"
    float duration = 0.0f;          // アニメーションの総時間

    // ★必ず time の昇順でソートされていること！
    std::vector<AnimNotifyEvent> events;

    template<class Archive>
    void serialize(Archive& archive) {
        archive(CEREAL_NVP(sequenceName), CEREAL_NVP(targetModelPath),
            CEREAL_NVP(targetAnimName), CEREAL_NVP(duration), CEREAL_NVP(events));
    }
};
