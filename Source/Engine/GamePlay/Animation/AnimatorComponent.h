#pragma once
#include "Engine/Assets/Model.h"
#include "Engine/GamePlay/Animation/Data/AnimSequence.h"
#include <vector>
// カーブデータ構造の先行宣言（後で定義します）
struct AnimationCurve;

// ===================================================================================
// ファイル: AnimatorComponent.h
// 概要: アニメーションの「再生状態」を保持するコンポーネント
// 
// [ 役割 ]
// AnimControllerSystem から渡された台本（AnimSequence）を記憶し、
// 現在の再生時間（タイマー）や、次に発火すべきイベントの栞（インデックス）を持つ。
// また、毎フレームの動的メモリ確保を避けるため、計算済みの骨格ポーズデータを
// キャッシュとして内部に保持する（DOD的最適化）。
// ===================================================================================

struct AnimatorComponent {
    // --- 再生する台本データ ---
    const AnimSequence* currentSequence = nullptr; // 共有データへのポインタ（8バイト）



    // 再生速度を動的に変化させるカーブのポインタ
    const AnimationCurve* activeCurve = nullptr;
	// ルートモーション速度倍率を制御するカーブのポインタ
    const AnimationCurve* activeRootMotionCurve = nullptr;

    // --- アニメーション状態（時計） ---
    float currentTimer = 0.0f;
    float playbackSpeed = 1.0f;
    bool  isLoop = true;
    bool  isFinished = false;
    bool disableRootMotion = false;

    // ★ O(1)アクセスのための「栞（しおり）」
    // 次にチェックするイベントのインデックス
    int nextEventIndex = 0;

    // =========================================================
    // クロスフェード・ブレンディング用データ
    // =========================================================
    const AnimSequence* previousSequence = nullptr;
    float previousTimer = 0.0f;
    float blendDuration = 0.0f;
    float currentBlendTime = 0.0f;
    bool isBlending = false;

    // --- 計算用キャッシュ (毎フレームのnew/deleteを防ぐマイボウル) ---
    std::vector<Model::NodePose> poseBufferA;
    std::vector<Model::NodePose> poseBufferB; // 過去のアニメーション用

    // エディタからの強制制御フラグ
    bool isEditorOverride = false;

    void Play(const AnimSequence* sequence, float blendTime = 0.2f, bool loop = true, float speed = 1.0f)
    {
        if (currentSequence != sequence) {
            // 前のアニメーションを「過去」として記録し、ブレンドを開始する
            if (currentSequence && blendTime > 0.001f) {
                previousSequence = currentSequence;
                previousTimer = currentTimer;
                isBlending = true;
                blendDuration = blendTime;
                currentBlendTime = 0.0f;
            }
            else {
                isBlending = false;
                previousSequence = nullptr;
            }

            currentSequence = sequence;
            currentTimer = 0.0f;
            nextEventIndex = 0;
            isFinished = false;
        }
        isLoop = loop;
        playbackSpeed = speed;
       
        // ★魔法: 自分の台本(sequence)から直接カーブのアドレスを取得する
        if (currentSequence) {
            activeCurve = &currentSequence->speedCurve;
            activeRootMotionCurve = &currentSequence->rootMotionCurve;
        }
        else {
            activeCurve = nullptr;
            activeRootMotionCurve = nullptr;
        }
    }

};