#include "TPSPlayerChainAttackSystem.h"
#include "TPSPlayerComponent.h"
#include "../PlayerStateComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX;
using namespace CCL::ECS;

void TPSPlayerChainAttackSystem::Update(float rawDt)
{

    ForEachWithID([&](EntityID id,
        TransformComponent& pTrans,
        TPSPlayerComponent& tps,
        TPSPlayerStateComponent& state,
        const PlayerStateTag::IsChainAttackTag tag,
        const TimeState& time) // ★追加
        {
            // ★各自の時計から dt を算出
            // これによりヒットストップ中はワープ移動も待機タイマーも完全に停止する
            float dt = time.localDt;

            auto* s = std::get_if<StateChainAttack>(&state.activeState);
            if (!s) return;


            // --- 1. 攻撃シーケンス中 ---
            if (s->currentIndex < s->targetCount)
            {
                CCL::ECS::EntityID targetID = s->targets[s->currentIndex];
                auto* targetTrans = _world->GetComponent<TransformComponent>(targetID);

                if (!targetTrans || !_world->IsEntityValid(targetID)) {
                    s->currentIndex++;
                    s->warpTimer = 0.0f;
                    s->isWaiting = false;
                    return;
                }

                // A. 待機フェーズ (敵の位置に到達した後)
                if (s->isWaiting)
                {
                    s->waitTimer += dt;
                    if (s->waitTimer >= s->config.pauseDuration)
                    {
                        s->waitTimer = 0.0f;
                        s->isWaiting = false;
                        s->currentIndex++; // 次の敵へ
                        s->warpTimer = 0.0f; // 移動タイマーリセット
                    }
                }
                // B. 移動フェーズ
                else
                {
                    s->warpTimer += dt;
                    if (s->warpTimer <= dt) s->startPosition = pTrans.position;

                    float t = (std::min)(s->warpTimer / s->config.warpInterval, 1.0f);

                    XMVECTOR start = XMLoadFloat3(&s->startPosition);
                    XMVECTOR end = XMLoadFloat3(&targetTrans->position);

                    // 移動（補完）
                    XMVECTOR currentPos = XMVectorLerp(start, end, t);
                    XMStoreFloat3(&pTrans.position, currentPos);

                    // 向きの更新
                    XMVECTOR dir = XMVector3Normalize(end - start);
                    if (XMVector3LengthSq(dir).m128_f32[0] > 0.0001f) {
                        float yaw = atan2f(XMVectorGetX(dir), XMVectorGetZ(dir));
                        XMStoreFloat4(&pTrans.rotation, XMQuaternionRotationRollPitchYaw(0, yaw, 0));
                        pTrans.isDirty = true;
                    }

                    // 到達判定
                    if (s->warpTimer >= s->config.warpInterval)
                    {
                        s->isWaiting = true; // 待機フェーズへ移行
                        s->waitTimer = 0.0f;

                        // ここで攻撃アニメーションの再生やヒットエフェクトを出すと効果的
                        // PlayAttackAnimation();
                        // SpawnHitEffect(targetTrans->position);
                    }
                }
            }
            // --- 2. フィニッシュ ---
            else
            {
                s->warpTimer += dt; // currentIndexが最大になった後はwarpTimerを汎用タイマーとして使用
                if (s->warpTimer >= s->config.finishDuration)
                {
                    _world->RequestRemoveComponent<PlayerStateTag::IsChainAttackTag>(id);
                    state.activeState = StateIdle{};
                    tps.canMove = true;
                }
            }
        });
}

REGISTER_LOGIC_SYSTEM(TPSPlayerChainAttackSystem, Priority::LogicStage::L02_Update);