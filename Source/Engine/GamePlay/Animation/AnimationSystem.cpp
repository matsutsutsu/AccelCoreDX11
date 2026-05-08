#include "AnimationSystem.h"
#include "ECS/Core/CCL_World.h"
//#include "Engine/Graphics/Resource/Model.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <cmath>
#include "Engine/Core/Math/StringHash.h"

// 先ほど作成した専用の手紙ヘッダー
#include "Engine/GamePlay/Animation/Data/AnimEventMessages.h"
#include "Engine/GamePlay/Animation/Data/AnimationCurve.h"

#include "Engine/GamePlay/Transform/Motion/MotionComponent.h"
#include "Game/Logics/System/BlackboardComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/Character/CharacterMovementInputComponent.h"

#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

// ===================================================================================
// ファイル: AnimationSystem.h
// 概要: アニメーションの「時間進行」と「イベント発火」を担う実働システム
// 
// [ 役割 ]
// AnimatorComponent の持つ時間を進め、指定された時間に応じた骨格計算をModelに依頼する。
// また、台本(AnimSequence)に書かれた時間と現在の時間を比較し、
// タイミングが合致したイベント（音、エフェクト、攻撃判定）を EventBus を通じて発行する。
// ===================================================================================

AnimationSystem::AnimationSystem() : IfSystem("AnimationSystem") {}

void AnimationSystem::Update(float rawDt)
{
    // ★超並列処理: ECSの真骨頂。数千体の計算を全コアに分散させる
    ForEachWithIDParallel([this](CCL::ECS::EntityID id, AnimatorComponent& animator, ModelComponent& modelComp, const TimeState& time) {

        // 台本（Sequence）がセットされていなければ何もしない
        if (!animator.currentSequence) return;

        // エディタで操作中の場合は、通常の時間進行とイベント発火をストップする
        if (animator.isEditorOverride) return;

        const AnimSequence* seq = animator.currentSequence;

        // ========================================================
        // 1. カーブによる時間の歪み（Time Warping）計算
        // ========================================================
        float curveMultiplier = 1.0f;

        if (animator.activeCurve) {
            // 現在の進捗率 (0.0=最初, 1.0=最後) を計算
            float progress = animator.currentTimer / seq->duration;

            // 進捗率をキーにして、カーブから現在の速度倍率をサンプリング
            curveMultiplier = animator.activeCurve->Evaluate(progress);
        }

        float prevTimeForDelta = animator.currentTimer;

        // 時間を進行させる (ベース速度 × カーブによる倍率 を dt に掛ける)
        animator.currentTimer += time.localDt * animator.playbackSpeed * curveMultiplier;

        float currTimeForDelta = animator.currentTimer;

        // ループを跨いでいたら、Delta計算を0にする
        if (currTimeForDelta < prevTimeForDelta) {
            prevTimeForDelta = currTimeForDelta;
        }

        // ========================================================
        //  過去の記憶（ブレンド）のタイマーを進行させる
        // ========================================================
        if (animator.isBlending) {
            animator.currentBlendTime += time.localDt;
            // 過去のアニメーションも再生し続ける（足がピタッと止まる不自然さを防ぐため）
            animator.previousTimer += time.localDt * animator.playbackSpeed;

            if (animator.previousSequence) {
                // 過去のアニメがループ仕様なら時間を丸め込む
                animator.previousTimer = fmod(animator.previousTimer, animator.previousSequence->duration);
            }

            // 指定した秒数が経過したらブレンド終了
            if (animator.currentBlendTime >= animator.blendDuration) {
                animator.isBlending = false;
                animator.previousSequence = nullptr;
            }
        }

        // ========================================================
        // 2. 高速イベント発火ロジック (O(1)アクセス)
        // ========================================================
        while (animator.nextEventIndex < seq->events.size()) {
            const auto& evt = seq->events[animator.nextEventIndex];

            // ★修正: 新しい仕様に合わせて evt.startTime を参照する
            if (animator.currentTimer < evt.startTime) {
                break; // まだ時間が来ていなければループを抜ける
            }

            uint32_t eventHash = CCL::Utils::HashString(evt.eventName.c_str());
            bool isStandardEvent = true;

            switch (eventHash) {
            case CCL::Utils::HashString("Play_Sound"):
                _world->GetEventBus().Publish(AnimEventSoundMessage{ id }); // 実際には param 等を渡す
                break;
            case CCL::Utils::HashString("Play_Effect"):
                _world->GetEventBus().Publish(AnimEventEffectMessage{ id, evt.stringParam });
                break;
            case CCL::Utils::HashString("HitBox"):
                // [重要] HitBox は CombatAnimationSyncSystem が区間監視するのでここでイベントは出さない
                break;
            default:
                isStandardEvent = false;
                break;
            }

            // --- B. ブラックボード特化の汎用処理 (エディタの命名規則に対応) ---
            if (!isStandardEvent) 
            {
                if (evt.eventName.find("BB_") == 0) {
                    if (auto* bb = _world->GetComponent<BlackboardComponent>(id)) 
                    {

                        // 1. 真偽値（Flag）の判定: "_True" または "_False" で終わるか
                        if (evt.eventName.find("_True") != std::string::npos) {
                            std::string key = evt.eventName.substr(3, evt.eventName.find("_True") - 3);
                            bb->SetBool(key, true);
                        }
                        else if (evt.eventName.find("_False") != std::string::npos) {
                            std::string key = evt.eventName.substr(3, evt.eventName.find("_False") - 3);
                            bb->SetBool(key, false);
                        }
                        // 2. 文字列パラメータがある場合は、そのままキーと値として登録
                        else if (!evt.stringParam.empty()) {
                            std::string key = evt.eventName.substr(3);
                            bb->SetString(key, evt.stringParam);
                        }
                    }
                }
            }

            // 栞を次に進める
            animator.nextEventIndex++;
        
        }

        // ========================================================
        // 3. ループと終了の判定
        // ========================================================
        if (animator.currentTimer >= seq->duration) {
            if (animator.isLoop) {
                // ループ時: 時間を丸め込み、イベントの栞を最初に戻す！
                animator.currentTimer = fmod(animator.currentTimer, seq->duration);
                animator.nextEventIndex = 0;
            }
            else {
                animator.currentTimer = seq->duration;
                animator.isFinished = true;
            }
        }

        // ========================================================
        // 4. 描画側(Model)への計算依頼
        // ========================================================
        if (modelComp.GetModel()) {
            Model* model = modelComp.GetModel();
            int animIndex = model->GetAnimationIndex(seq->targetAnimName.c_str());

            if (animIndex != -1) {
                // ルートモーション抽出フラグの判定（Jolt環境にも対応）
                auto* motion = _world->GetComponent<MotionComponent>(id);
                auto* charInput = _world->GetComponent<CharacterMovementInputComponent>(id);
                bool extractMode = (motion != nullptr || charInput != nullptr) && (!animator.disableRootMotion);

                DirectX::XMVECTOR deltaVec = DirectX::XMVectorZero();

                // ブレンド中の場合は2つのアニメーションを計算して混ぜる
                if (animator.isBlending && animator.previousSequence) {
                    int prevAnimIndex = model->GetAnimationIndex(animator.previousSequence->targetAnimName.c_str());

                    if (prevAnimIndex != -1) {
                        // ★ DX12と同じく、毎フレーム確実に String (c_str) でボーンを指定する！
                        model->ComputeAnimationWithDelta(
                            prevAnimIndex, animator.previousTimer, animator.previousTimer,
                            animator.poseBufferB, extractMode, animator.rootNodeName.c_str(), nullptr
                        );

                        model->ComputeAnimationWithDelta(
                            animIndex, currTimeForDelta, prevTimeForDelta,
                            animator.poseBufferA, extractMode, animator.rootNodeName.c_str(), extractMode ? &deltaVec : nullptr
                        );

                        float blendRate = std::fmax(0.0f, std::fmin(animator.currentBlendTime / animator.blendDuration, 1.0f));
                        Model::BlendAnimations(animator.poseBufferB, animator.poseBufferA, blendRate, animator.poseBufferA);
                    }
                    else {
                        model->ComputeAnimationWithDelta(
                            animIndex, currTimeForDelta, prevTimeForDelta,
                            animator.poseBufferA, extractMode, animator.rootNodeName.c_str(), extractMode ? &deltaVec : nullptr
                        );
                    }
                }
                else {
                    // 通常の単独再生
                    // ★ DX12と同じく、毎フレーム確実に String (c_str) でボーンを指定する！
                    model->ComputeAnimationWithDelta(
                        animIndex, currTimeForDelta, prevTimeForDelta,
                        animator.poseBufferA, extractMode, animator.rootNodeName.c_str(), extractMode ? &deltaVec : nullptr
                    );
                }

                // ========================================================
                // ★ 抽出された移動量の加工と適用
                // ========================================================
                if (extractMode) {
                    using namespace DirectX::SimpleMath;

                    float rmMultiplier = 1.0f;
                    float normalizedTime = seq->duration > 0.0f ? (animator.currentTimer / seq->duration) : 0.0f;

                    if (animator.activeRootMotionCurve && animator.activeRootMotionCurve->keys.size() > 0) {
                        rmMultiplier = animator.activeRootMotionCurve->Evaluate(normalizedTime);
                    }

                    Vector3 localDelta;
                    DirectX::XMStoreFloat3(&localDelta, deltaVec);

                    localDelta.x *= rmMultiplier;
                    localDelta.z *= rmMultiplier;

                    auto* trans = _world->GetComponent<TransformComponent>(id);
                    if (trans) {
                        localDelta = Vector3::Transform(localDelta, trans->rotation);
                    }

                    DirectX::XMFLOAT3 finalDelta(localDelta.x, localDelta.y, localDelta.z);

                    // ★ デバッグログ1：抽出された純粋な移動量
                    CCL_LOG_INFO(LogCategory::Game, "[RootMotion] Entity[%llu] Extracted Delta: X=%.4f, Y=%.4f, Z=%.4f",
                        id, finalDelta.x, finalDelta.y, finalDelta.z);

                    // 旧Rigidbodyと新JoltCharacterのどちらでも動くように分配
                    if (motion) {
                        motion->AddImpulse(finalDelta);
                        CCL_LOG_INFO(LogCategory::Game, "[RootMotion] -> Applied to MotionComponent");
                    }
                    else if (charInput && time.localDt > 0.0f) {
                        charInput->desiredVelocity.x += finalDelta.x / time.localDt;
                        charInput->desiredVelocity.y += finalDelta.y / time.localDt;
                        charInput->desiredVelocity.z += finalDelta.z / time.localDt;
                        // ★ デバッグログ2：JoltCharacterに渡された後の速度
                        CCL_LOG_INFO(LogCategory::Game, "[RootMotion] -> Applied to CharacterInput, Velocity: X=%.2f, Z=%.2f",
                            charInput->desiredVelocity.x, charInput->desiredVelocity.z);
                    }
                    else {
                        // ★ デバッグログ3：受け取り手がいない場合の警告
                        CCL_LOG_WARN(LogCategory::Game, "[RootMotion] ERROR: No Motion or CharacterInput Component found!");
                    }
                }

                // 最後に描画用の骨格姿勢を確定させる
                model->SetNodePoses(animator.poseBufferA);

            }
        }
        });
}


// エディタ（AnimationSequencerWindow）から呼ばれる手動更新処理
void AnimationSystem::UpdateManual(CCL::ECS::EntityID entity, float time)
{
    auto* animator = _world->GetComponent<AnimatorComponent>(entity);
    auto* modelComp = _world->GetComponent<ModelComponent>(entity);

    if (!animator || !modelComp || !modelComp->GetModel()) return;

    Model* model = modelComp->GetModel();
    const AnimSequence* seq = animator->currentSequence;
    if (!seq) return;

    int animIndex = model->GetAnimationIndex(seq->targetAnimName.c_str());

    if (animIndex != -1) {
        // ★ ここも String (c_str) 指定に差し戻す
        model->ComputeAnimationWithDelta(
            animIndex, time, time, // エディタ中はデルタ移動しない
            animator->poseBufferA, animator->disableRootMotion, animator->rootNodeName.c_str(), nullptr
        );
        model->SetNodePoses(animator->poseBufferA);
    }
}

// 修正後 (L02_Update でAIや移動と共にアニメーション時間を進める)
REGISTER_LOGIC_SYSTEM(AnimationSystem, Priority::LogicStage::L02_Update);