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

void AnimationSystem::Update(float dt)
{
    // ★超並列処理: ECSの真骨頂。数千体の計算を全コアに分散させる
    ForEachWithIDParallel([dt, this](CCL::ECS::EntityID id, AnimatorComponent& animator, ModelComponent& modelComp) {

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

        // 時間を進行させる (ベース速度 × カーブによる倍率 を dt に掛ける)
        animator.currentTimer += dt * animator.playbackSpeed * curveMultiplier;

        // ========================================================
        //  過去の記憶（ブレンド）のタイマーを進行させる
        // ========================================================
        if (animator.isBlending) {
            animator.currentBlendTime += dt;
            // 過去のアニメーションも再生し続ける（足がピタッと止まる不自然さを防ぐため）
            animator.previousTimer += dt * animator.playbackSpeed; 

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

            // まだイベントの時間に達していなければ、これ以上の走査は不要
            // 点のイベント（音を鳴らす、エフェクトを出す）は開始時間になった瞬間に1回だけ発火する
            if (animator.currentTimer >= evt.startTime) {

                // ここがAAAの設計！
                // 文字列を数値に変換し、switch文で最速のジャンプを行う
                // ※実行時にはただの「数値の比較（int == int）」になるため負荷はゼロに等しい
                uint32_t eventHash = CCL::Utils::HashString(evt.eventName.c_str());

                switch (eventHash) {
                case  CCL::Utils::HashString("HitBox_Start"):
                    _world->GetEventBus().Publish(AnimEventHitBoxMessage{ id, true });
                    break;
                case  CCL::Utils::HashString("HitBox_End"):
                    _world->GetEventBus().Publish(AnimEventHitBoxMessage{ id, false });
                    break;
                case  CCL::Utils::HashString("Play_Sound"):
                    _world->GetEventBus().Publish(AnimEventSoundMessage{ id });
                    break;
                case  CCL::Utils::HashString("Play_Effect"):
                    _world->GetEventBus().Publish(AnimEventEffectMessage{ id, evt.stringParam });
                    break;
				case CCL::Utils::HashString("HitBox"):  // 状態を持つイベントはシステム側で検知するのでイベントはいらない
                    // [重要] Hitbox は CombatAnimationSyncSystem が直接「状態」として
                    // 読み取って処理するため、イベントバスに投げる必要はない。無視してよい。
                    break;
                default:
                    // 未知のイベントは無視するか、警告を出す
                    break;
                }

                // 栞を次に進める
                animator.nextEventIndex++;
            }
            else {
                break; // まだ時間が来ていないイベントに当たったらループを抜ける
            }
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
                // ブレンド中の場合は2つのアニメーションを計算して混ぜる
                if (animator.isBlending && animator.previousSequence) {
                    int prevAnimIndex = model->GetAnimationIndex(animator.previousSequence->targetAnimName.c_str());

                    if (prevAnimIndex != -1) {
                        // ① 過去の姿勢を B に保存
                        model->ComputeAnimation(prevAnimIndex, animator.previousTimer, animator.poseBufferB);
                        // ② 現在の姿勢を A に保存
                        model->ComputeAnimation(animIndex, animator.currentTimer, animator.poseBufferA);

                        // ③ 補間率 (0.0 ~ 1.0) を算出してクランプ
                        float blendRate = animator.currentBlendTime / animator.blendDuration;
                        blendRate = std::fmax(0.0f, std::fmin(blendRate, 1.0f));

                        // ④ BとAを混ぜ合わせ、結果を A に上書きする！
                        Model::BlendAnimations(animator.poseBufferB, animator.poseBufferA, blendRate, animator.poseBufferA);

                        // ⑤ 混ぜた結果を骨に適用
                        model->SetNodePoses(animator.poseBufferA);
                    }
                    else {
                        // 過去のデータが見つからなければ通常再生
                        model->ComputeAnimation(animIndex, animator.currentTimer, animator.poseBufferA);
                        model->SetNodePoses(animator.poseBufferA);
                    }
                }
                else {
                    // ★ 通常の単独再生
                    model->ComputeAnimation(animIndex, animator.currentTimer, animator.poseBufferA);
                    model->SetNodePoses(animator.poseBufferA);
                }
            }
        }
        });
}


// エディタ（AnimationSequencerWindow）から呼ばれる手動更新処理
void AnimationSystem::UpdateManual(CCL::ECS::EntityID entity, float time)
{
    // 1. 対象のエンティティから必要なコンポーネントを取得
    auto* animator = _world->GetComponent<AnimatorComponent>(entity);
    auto* modelComp = _world->GetComponent<ModelComponent>(entity);

    if (!animator || !modelComp || !modelComp->GetModel()) return;
    
    Model* model = modelComp->GetModel();
    
    // 2. 現在読んでいる台本（エディタで編集中）を取得
    const AnimSequence* seq = animator->currentSequence;
    if (!seq) return;

    // 3. 台本に書かれているアニメーション名から、3Dモデル側のインデックスを引く
    int animIndex = model->GetAnimationIndex(seq->targetAnimName.c_str());
    
    // 4. 指定された時間（time）で骨の姿勢を計算し、モデルに適用する
    if (animIndex != -1) {
        model->ComputeAnimation(animIndex, time, animator->poseBufferA);
        model->SetNodePoses(animator->poseBufferA);
    }
}

// 修正後 (L02_Update でAIや移動と共にアニメーション時間を進める)
REGISTER_LOGIC_SYSTEM(AnimationSystem, Priority::LogicStage::L02_Update);