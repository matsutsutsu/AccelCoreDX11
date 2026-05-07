#include "TPSPlayerStateSystem.h"

#include "ECS/Core/CCL_World.h"
#include <DirectXMath.h>
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Core/Math/StringHash.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "TPSPlayerComponent.h"
#include "../PlayerStateComponent.h"
#include "Game/Logics/Character/CharacterMovementInputComponent.h"
#include "Game/Logics/Combat/StaminaComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
using namespace DirectX;


void TPSPlayerStateSystem::Update(float rawDt)
{
    // カメラの方向を取得（入力方向の計算用）
    Camera* mainCamera = _world->HasResource<Camera*>() ? _world->GetResource<Camera*>() : nullptr;
    XMVECTOR camForward = XMVectorSet(0, 0, 1, 0);
    XMVECTOR camRight = XMVectorSet(1, 0, 0, 0);

    if (mainCamera) {
        // 1. カメラのビュー行列（XMFLOAT4X4）を取得し、SIMDレジスタ（XMMATRIX）にロード（Load）する
        XMMATRIX viewMat = XMLoadFloat4x4(&mainCamera->GetView());

        // 2. SIMDレジスタ上で逆行列計算を行う
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);
        camForward = XMVector3Normalize(XMVectorSetY(invView.r[2], 0.0f));
        camRight = XMVector3Normalize(XMVectorSetY(invView.r[0], 0.0f));
    }

    ForEachWithID([&](CCL::ECS::EntityID id,
        TPSPlayerComponent& tps,
        TPSPlayerStateComponent& state,
        AnimParametersComponent& animParams,
        StaminaComponent& stam,
        const TimeState& time)
        {
            float dt = time.localDt; // ★各自の時間を使用

            state.stateTimer += dt;

            // 入力方向の計算
            XMVECTOR moveDir = XMVectorAdd(
                XMVectorScale(camForward, tps.input.moveInput.y),
                XMVectorScale(camRight, tps.input.moveInput.x)
            );
            if (XMVector3LengthSq(moveDir).m128_f32[0] > 0.001f) {
                moveDir = XMVector3Normalize(moveDir);
            }
            else {
                // 入力がない場合は現在の前方（例としてtpsが保持している方向など）を使用
                // ここでは簡易的にcamForwardを代用s
            }
            // --- ステートマシンロジック ---
            std::visit([&](auto& s) 
                {
                using T = std::decay_t<decltype(s)>;

                // 1. Idle / Move 状態
                if constexpr (std::is_same_v<T, StateIdle> || std::is_same_v<T, StateMove>)
                {
                    tps.moveRate = 1.0f;

                    // 反撃猶予コンポーネントがある場合
                    if (auto* opportunity = _world->GetComponent<PlayerStateTag::CounterAttackOpportunity>(id))
                    {
                        // 1. 猶予タイマーの更新
                        opportunity->remainingTime -= dt;

                        // 2. 入力受付：反撃可能
                        if (tps.input.isAttackTriggered) {
                            StateAttack next;
                            next.config = state.configs.counterAttack;
                            next.comboCount = 99; // カウンター技
                            tps.currentSpeed = 0;
                            // 遷移処理（タグ付与・ステート変更）
                            _world->AddComponent<PlayerStateTag::IsAttackingTag>(id);
                            state.activeState = next;
                            state.stateTimer = 0.0f;
                            animParams.SetTrigger(CCL::Utils::HashString("CounterAttack"));

                            // 権利を使い切ったのでコンポーネント削除
                            _world->RequestRemoveComponent<PlayerStateTag::CounterAttackOpportunity>(id);
                            return;
                        }

                        // 3. 時間切れで削除
                        if (opportunity->remainingTime <= 0.0f) {
                            _world->RequestRemoveComponent<PlayerStateTag::CounterAttackOpportunity>(id);
                        }
                    }

                    if (tps.input.isDodgeTriggered)
                    {
                        // --- Dodge開始処理 ---
                        StateDodge nextState; // 構造体のデフォルト値を使用
                        nextState.config = state.configs.dodge;
                        // 1. 疲労状態ではない
                        // 2. 現在のスタミナ - 予約済み消費量 がコスト以上ある
                        bool canDodge = !stam.isFatigued &&
                            ((stam.current - stam.instantConsumeRequest) >= nextState.config.usedStamina);

                        if (canDodge)
                        {
                            // 条件クリア：消費予約とステート遷移を実行
                            stam.instantConsumeRequest += nextState.config.usedStamina;
                            animParams.SetTrigger(CCL::Utils::HashString(tps.config.action.dodge));
                            _world->AddComponent<PlayerStateTag::IsDashingTag>(id);
                            state.activeState = nextState;
                            state.stateTimer = 0.0f;
                            state.isFinished = false;
                            return;
                        }
                        else
                        {
                            // スタミナ不足：SEを鳴らしたり、「スタミナ不足」のUIを出すならここ
                            return;
                        }
                        return;
                    }

                    if (tps.input.isTargeting)
                    {

                        StateTargeting next;
                        next.config = state.configs.Targeting;
                        state.activeState = next;
                        state.stateTimer = 0.0f;

                        // 1. TPSカメラから回転の値を取得
                        bool canTargetMode = !stam.isFatigued;
                        if (!canTargetMode)
                        {
                            return;
                        }

                        float targetRotationY = 0.0f;
                        auto tpsCameras = _world->View<CameraBodyTPS>();

                        for (auto tpsId : tpsCameras)
                        {
                            auto tpsTrans = _world->GetComponent<TransformComponent>(tpsId);
                            targetRotationY = tpsTrans->rotation.y;
                            break; // 複数のTPSカメラがある場合、最初のカメラの回転を採用
                        }

                        // 2. FPSカメラへその回転値を適用
                        auto fpsCameras = _world->View<CameraBodyFPS>();
                        for (auto fpsId : fpsCameras)
                        {
                            auto fpsTrans = _world->GetComponent<TransformComponent>(fpsId);
                            auto fpsVCamera = _world->GetComponent<VirtualCamera>(fpsId);

                            // TPSカメラの回転をコピー
                            fpsTrans->rotation.y = targetRotationY;

                            // FPSカメラを有効化
                            fpsVCamera->priority = 999;
                        }
                        state.isFinished = false;
                        _world->AddComponent<PlayerStateTag::IsLockOnTag>(id);
                        return;
                    }

                    // TPSPlayerStateSystem.cpp 内の攻撃遷移部分
                    if (tps.input.isAttackTriggered && !tps.input.isTargeting)
                    {
                        StateAttack next;

                        if (!tps.isGrounded)
                        {
                            if (tps.hasPerformedAirAttack) return;

                            next.config = state.configs.airAttack;
                            next.isAirAttack = true;
                            next.config.maxComboCount = 1;
                            tps.hasPerformedAirAttack = true;

                            // 【修正】空中攻撃であることを明示的にトリガーする（JSON側もこれに合わせるのが楽です）
                            // もしくは、既存の "Attack_1" を使うなら、その前にジャンプ状態であることを保証する
                            animParams.SetTrigger(CCL::Utils::HashString("Attack_Air"));
                        }
                        else
                        {
                            next.config = state.configs.attack;
                            next.isAirAttack = false;
                            animParams.SetTrigger(CCL::Utils::HashString("Attack_1"));
                        }

                        next.comboCount = 1;
                        //tps.canMove = false;
                        _world->AddComponent<PlayerStateTag::IsAttackingTag>(id);
                        state.activeState = next;
                        state.stateTimer = 0.0f;
                        state.isFinished = false;
                        return;
                    }
                }
                else if constexpr (std::is_same_v<T, StateAttack>)
                {

                    tps.moveRate = 0.0025f;
                    if (state.isFinished)
                    {
                        auto attack = static_cast<StateAttack>(s);
                        if (attack.hasDodgeBuffered)
                        {

                            // --- Dodge開始処理 ---
                            StateDodge nextState; // 構造体のデフォルト値を使用
                            nextState.config = state.configs.dodge;
                            // 1. 疲労状態ではない
                            // 2. 現在のスタミナ - 予約済み消費量 がコスト以上ある
                            bool canDodge = !stam.isFatigued &&
                                ((stam.current - stam.instantConsumeRequest) >= nextState.config.usedStamina);

                            if (canDodge)
                            {
                                // 条件クリア：消費予約とステート遷移を実行
                                stam.instantConsumeRequest += nextState.config.usedStamina;
                                animParams.SetTrigger(CCL::Utils::HashString(tps.config.action.dodge));
                                _world->AddComponent<PlayerStateTag::IsDashingTag>(id);
                                state.activeState = nextState;
                                state.stateTimer = 0.0f;
                                state.isFinished = false;
                                return;
                            }
                        }
                        animParams.SetTrigger(CCL::Utils::HashString("AttackEnd"));
                        _world->RequestRemoveComponent<PlayerStateTag::IsAttackingTag>(id);
                        StateIdle next;
                        state.activeState = next;
                        state.stateTimer = 0.0f;
                    }
                }
                // 2. Dodge 状態 (監視のみ)
                else if constexpr (std::is_same_v<T, StateDodge>) 
                {
                    tps.moveRate = 1.0f;
                    if (state.stateTimer > s.config.duration)
                    {
                        // --- Dodge終了処理 ---
                        // 慣性を残すため、現在の速度をTPSComponentへ移譲
                        tps.currentSpeed = s.config.dashSpeed;

                        _world->RequestRemoveComponent<PlayerStateTag::IsDashingTag>(id);

                        state.activeState = StateIdle{};
                        state.stateTimer = 0.0f;
                    }
                }
                // ChainAttack 状態
                else if constexpr (std::is_same_v<T, StateChainAttack>)
                {
                    tps.canMove = false;
                    tps.moveRate = 0.01f;
                }
                // LockOn 状態
                else if constexpr (std::is_same_v<T, StateTargeting>)
                {
                    
                    //ねらっているあいだは動けないように
                    tps.canMove = false;
                    tps.moveRate = 0.0f;
                    // スタミナ消費
                    stam.current -= s.config.staminaCostPerSec * dt;
                    stam.isConsuming = true;

                    // 攻撃実行への遷移 (データは LockOnSystem が更新済み)
                    if (((stam.current < 0) || (tps.input.isAttackTriggered)) && s.targetCount > 0)
                    {
                        StateChainAttack next{ state.configs.chain };
                        next.targetCount = s.targetCount;
                        for (int i = 0; i < s.targetCount; ++i) next.targets[i] = s.targets[i];

                        auto cameras = _world->View<CameraBodyFPS>();
                        for (auto id : cameras)
                        {
                            auto vCameraComp = _world->GetComponent<VirtualCamera>(id);
                            vCameraComp->priority = 0;
                        }

                        tps.canMove = true;
                        state.activeState = next;
                        _world->RequestRemoveComponent<PlayerStateTag::IsLockOnTag>(id);
                        _world->AddComponent<PlayerStateTag::IsChainAttackTag>(id);
                        return;
                    }
                    else if (((stam.current < 0) || (tps.input.isAttackTriggered)) && s.targetCount <= 0)
                    {
                        _world->RequestRemoveComponent<PlayerStateTag::IsLockOnTag>(id);
                        state.activeState = StateIdle{};
                        auto cameras = _world->View<CameraBodyFPS>();
                        for (auto id : cameras)
                        {
                            auto vCameraComp = _world->GetComponent<VirtualCamera>(id);
                            vCameraComp->priority = 0;
                        }
                        tps.canMove = true;
                        state.isFinished = false;
                        return;
                    }
                }
                }, state.activeState);
        });
}

REGISTER_LOGIC_SYSTEM(TPSPlayerStateSystem, Priority::LogicStage::L02_Update); // 状態更新の直後に実行