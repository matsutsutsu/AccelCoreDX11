#include "TPSPlayerMoveSystem.h"
#include "TPSPlayerComponent.h"
#include "../PlayerStateComponent.h"

#include "Engine/Physics/IPhysicsAPI.h" // 物理APIの窓口
#include "Engine/Core/Math/StringHash.h" // HashString用
#include "Engine/Graphics/Core/Camera.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "Game/Logics/Combat/StaminaComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/System/Modifier/ModifierComponent.h"
#include "Game/Logics/Character/CharacterMovementInputComponent.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "Engine/GamePlay/Transform//Motion/MotionComponent.h"
#include <memory>

#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

// Joltキャラクターの接地状態を知るために必要
#include "Engine/GamePlay/Physics/Character/JoltCharacterHandleComponent.h"

using namespace DirectX;
using namespace CCL::ECS;

void TPSPlayerMoveSystem::Update(float rawDt)
{
    // 1. 物理APIとカメラリソースの取得
    if (!_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;
    auto physics = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();

    // 1. カメラリソースの取得（移動方向の基準）
    Camera* mainCamera = _world->HasResource<Camera*>() ? _world->GetResource<Camera*>() : nullptr;
    DirectX::XMVECTOR camForward = DirectX::XMVectorSet(0, 0, 1, 0);
    DirectX::XMVECTOR camRight = DirectX::XMVectorSet(1, 0, 0, 0);

    if (mainCamera) {
        // 1. カメラのビュー行列（XMFLOAT4X4）を取得し、SIMDレジスタ（XMMATRIX）にロード（Load）する
        XMMATRIX viewMat = XMLoadFloat4x4(&mainCamera->GetView());

        // 2. SIMDレジスタ上で逆行列計算を行う
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);
        camForward = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(invView.r[2], 0.0f));
        camRight = DirectX::XMVector3Normalize(DirectX::XMVectorSetY(invView.r[0], 0.0f));
    }

    ForEachWithID([&](CCL::ECS::EntityID entityID,
        TransformComponent& trans,
        TPSPlayerComponent& tps,
        TPSPlayerStateComponent& state,
        StaminaComponent& stam,
        AnimParametersComponent& animParams,
        CharacterMovementInputComponent& moveInput,
        const ModifierStatusComponent& modStatus,
        const TimeState& time)
        {
            float dt = time.localDt; // ★各自の時間を使用
            auto* motion = _world->GetComponent<MotionComponent>(entityID);

            // ========================================================
            // ★ ここに接地判定とジャンプ予約のコードを追加
            // ========================================================
            auto* joltChar = _world->GetComponent<JoltCharacterHandleComponent>(entityID);

            bool isGrounded = false;
            if (joltChar && joltChar->character) {
                isGrounded = (joltChar->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround);
            }
            // 現在の接地状態を更新（これ以降 tps.isGrounded は「今フレームの状態」になる）
            tps.isGrounded = isGrounded;


            // ★ 接地している場合のみリセットを行う
            if (tps.isGrounded)
            {
                tps.hasPerformedAirAttack = false;
            }
            // ジャンプの意思決定
            moveInput.jumpRequested = false;
            if (tps.isGrounded && tps.input.isJumpTriggered && tps.canMove)
            {
                moveInput.jumpRequested = true;
                static const uint32_t JUMP_HASH = CCL::Utils::HashString("Jump");
                animParams.SetTrigger(JUMP_HASH);
            }


            // アニメーションへの状態伝達 (AnimGraph用)
            static const uint32_t IS_FALLING_HASH = CCL::Utils::HashString("IsFalling");
            animParams.SetBool(IS_FALLING_HASH, !tps.isGrounded);
            // ========================================================
            // ★ 追加ここまで
            // ========================================================

            // 状態フラグの確認
            bool isAttacking = _world->HasComponent<PlayerStateTag::IsAttackingTag>(entityID);
            bool isDashing = _world->HasComponent<PlayerStateTag::IsDashingTag>(entityID);       

            // --- 1. 入力ベクトルの計算 ---
            bool hasInput = (std::abs(tps.input.moveInput.y) > 0.001f || std::abs(tps.input.moveInput.x) > 0.001f);
            bool canMove = tps.canMove && !isAttacking;

            // --- 2. 攻撃時の踏み込み(Lunge)処理 ---
            if (isAttacking) {
                if (auto* s = std::get_if<StateAttack>(&state.activeState)) 
                {

                        // 踏み込み終了後は停止
                        moveInput.desiredVelocity = Vector3::Zero;
                }
            }

            Vector3 moveDir = Vector3::Zero;
            if (hasInput && canMove) {
                moveDir = (camForward * tps.input.moveInput.y) + (camRight * tps.input.moveInput.x);
                moveDir.Normalize();
            }

            // --- 2. 目標速度の決定 (スタミナ考慮) ---
            bool canSprint = (hasInput && canMove && tps.input.isSprintHeld && !stam.isFatigued);
            stam.isConsuming = canSprint;

            float targetSpeed = (hasInput && canMove) ? (canSprint ? tps.runSpeed : tps.walkSpeed) : 0.0f;
            targetSpeed *= tps.moveRate;

            // 加減速 (tps.currentSpeed を更新)
            // 修正ポイント：isDashing 中もこの計算を通すことで、回避アニメーション中に背後で速度が RunSpeed まで上がる
            if (tps.currentSpeed < targetSpeed)
                tps.currentSpeed = (std::min)(tps.currentSpeed + tps.accel * dt, targetSpeed);
            else
                tps.currentSpeed = (std::max)(tps.currentSpeed - 40.0f * dt, targetSpeed);

            // --- 3. moveInput への書き込み ---

            // 通常移動 or 回避(Dodge)の速度合成
            if (isDashing) {
                if (auto* s = std::get_if<StateDodge>(&state.activeState)) {
                    XMVECTOR dDir = XMLoadFloat3(&s->dashDir);
                    Vector3 finalDashDir = Vector3(XMVectorGetX(dDir), XMVectorGetY(dDir), XMVectorGetZ(dDir));

                    // 回避中は、固定の回避速度を使用
                    moveInput.desiredVelocity = finalDashDir * s->config.dashSpeed;
                    moveInput.desiredLookDir = finalDashDir;

                    // 特殊対応: 回避終了の瞬間に currentSpeed が低いと失速するので
                    // 回避中に入力がある場合は、強制的に currentSpeed を RunSpeed などの高値で維持しておく
                    if (hasInput && canMove) {
                        // 回避が終わった瞬間に即座に最高速で走り出せるようにする
                        tps.currentSpeed = targetSpeed;
                    }
                }
            }
            else {
                // 通常移動
                moveInput.desiredVelocity = moveDir * tps.currentSpeed;
                if (moveDir.LengthSquared() > 0.001f) {
                    moveInput.desiredLookDir = moveDir;
                }
            }

            // ★ 攻撃中（カウンター含む）は回避の移動計算を完全に無効化
            if (_world->HasComponent<PlayerStateTag::IsAttackingTag>(entityID)) {
                tps.currentSpeed = 0.0f; // 内部速度をリセット
            }

            if (tps.forceStopMovement)
            {
                moveInput.desiredVelocity = { 0, 0, 0 };
                tps.currentSpeed = 0.0f;       // 加速計算もリセット
                tps.forceStopMovement = false; // 1フレーム適用したら解除
            }

            // ========================================================
            // ★ 追加: 進行方向への回転処理 (Y軸)
            // ========================================================
            if (moveDir.LengthSquared() > 0.001f)
            {
                // 1. 目標の方向ベクトルから角度(ラジアン)を計算
                // DirectXの座標系に合わせ、atan2(x, z) を使用
                float targetAngle = atan2f(moveDir.x, moveDir.z);

                // 2. クォータニオンの作成
                XMVECTOR targetQuat = XMQuaternionRotationRollPitchYaw(0.0f, targetAngle, 0.0f);
                XMVECTOR currentQuat = XMLoadFloat4(&trans.rotation);

                // 3. 補間（回転速度 rotationSpeed は適宜 TPSPlayerComponent 等に定義してください）
                // 例: 10.0f * dt で滑らかに回転
                // 4. Transformに保存
                float rotationSpeed = 60.0f;
                XMVECTOR nextQuat = XMQuaternionSlerp(currentQuat, targetQuat, (std::min)(1.0f, rotationSpeed * dt));

                XMStoreFloat4(&trans.rotation, nextQuat);
            }
          

            // --- 4. アニメーションパラメータ ---
            static const uint32_t SPEED_HASH = CCL::Utils::HashString("Speed");
            animParams.SetFloat(SPEED_HASH, tps.currentSpeed);

            // ========================================================
             // ★ 統合された蓄積移動量の適用 (ゼロ除算ガード)
             // ========================================================
            if (motion)
            {
                XMVECTOR pending = XMLoadFloat3(&motion->pendingMovement);
                if (XMVector3LengthSq(pending).m128_f32[0] > 0.0001f)
                {
                    XMVECTOR currentVel = XMLoadFloat3(&moveInput.desiredVelocity);

                    // ★修正: dt が 0.0f の場合（完全停止中）は、速度への変換を行わず維持する
                    if (dt > 0.0001f) {
                        XMVECTOR extraVel = XMVectorScale(pending, 1.0f / dt);
                        XMStoreFloat3(&moveInput.desiredVelocity, XMVectorAdd(currentVel, extraVel));

                        // 適用したのでリセット
                        motion->pendingMovement = { 0.0f, 0.0f, 0.0f };
                    }
                    else {
                        // 時間が止まっている間は移動速度をゼロにしておく
                        moveInput.desiredVelocity = { 0.0f, 0.0f, 0.0f };
                    }
                }
            }
        });
}

// システムの登録
REGISTER_LOGIC_SYSTEM(TPSPlayerMoveSystem, Priority::LogicStage::L02_Update);