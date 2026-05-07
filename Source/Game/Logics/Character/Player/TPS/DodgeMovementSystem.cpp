#include "DodgeMovementSystem.h"
#include "TPSPlayerComponent.h"
#include "../PlayerStateComponent.h" // TagやDashParamsの定義場所
#include "Engine/Physics/IPhysicsAPI.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Game/Logic/Combat/StaminaComponent.h"
#include "Engine/Graphics/Camera.h"

using namespace DirectX;

void DodgeMovementSystem::Update(float rawDt)
{
    if (!_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;
    auto physics = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();

    // カメラ方向の取得（入力補正用）
    Camera* mainCamera = _world->HasResource<Camera*>() ? _world->GetResource<Camera*>() : nullptr;
    XMVECTOR camForward = XMVectorSet(0, 0, 1, 0);
    XMVECTOR camRight = XMVectorSet(1, 0, 0, 0);

    if (mainCamera) {
        XMMATRIX invView = XMMatrixInverse(nullptr, mainCamera->GetView());
        camForward = XMVector3Normalize(XMVectorSetY(invView.r[2], 0.0f));
        camRight = XMVector3Normalize(XMVectorSetY(invView.r[0], 0.0f));
    }

    ForEachWithID([&](CCL::ECS::EntityID entityID,
        TransformComponent& trans,
        TPSPlayerComponent& tps,
        TPSPlayerStateComponent& state,
        PlayerStateTag::IsDashingTag& tag,
        const TimeState& time)
        {
            float dt = time.localDt; // ★各自の時間を使用
            auto* s = std::get_if<StateDodge>(&state.activeState);
            if (!s) return;

            // --- 1. 初回フレーム：基準方向（baseDashDir）の確定 ---
            if (!s->isInitialized) 
            {

                    // 入力に基づく方向計算
                    XMVECTOR initialDir = XMVectorAdd(
                        XMVectorScale(camForward, tps.input.moveInput.y),
                        XMVectorScale(camRight, tps.input.moveInput.x)
                    );

                    // 入力がない場合
                    if (XMVector3LengthSq(initialDir).m128_f32[0] < 0.001f)
                    {
                        // キャラクターの現在の向き（Transformの回転）から前方ベクトルを取得
                        XMVECTOR charQuat = XMLoadFloat4(&trans.rotation);
                        XMVECTOR charForward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), charQuat);

                        // カメラの正面方向とキャラクターの正面方向をミックス、あるいはキャラクターの向きを優先
                        // ここでは「キャラクターが向いている方向」をベースにする
                        initialDir = XMVector3Normalize(XMVectorSetY(charForward, 0.0f));

                        // もしキャラクターの向きが不正なら最終手段としてカメラの正面
                        if (XMVector3LengthSq(initialDir).m128_f32[0] < 0.001f) {
                            initialDir = camForward;
                        }
                    }
                    else
                    {
                        initialDir = XMVector3Normalize(initialDir);
                    }
                XMStoreFloat3(&s->baseDashDir, initialDir);
                XMStoreFloat3(&s->dashDir, initialDir); // 初期は dashDir も同じ
                s->isInitialized = true;
            }

            // --- 2. 旋回制限付きの方向更新 ---
            XMVECTOR inputVec = XMVector3Normalize(XMVectorAdd(
                XMVectorScale(camForward, tps.input.moveInput.y),
                XMVectorScale(camRight, tps.input.moveInput.x)
            ));

            if (XMVector3LengthSq(inputVec).m128_f32[0] > 0.001f)
            {
                XMVECTOR baseDir = XMLoadFloat3(&s->baseDashDir);
                XMVECTOR currentDir = XMLoadFloat3(&s->dashDir);

                // A. 入力方向に少しずつ近づける（操作介入）
                XMVECTOR nextDir = XMVector3Normalize(XMVectorLerp(currentDir, inputVec, 0.4f * dt * 60.0f));

                // B. 基準方向からの角度差をチェック
                // dot積から角度を求める
                float dot = XMVectorGetX(XMVector3Dot(baseDir, nextDir));
                float angle = acosf(std::clamp(dot, -1.0f, 1.0f));

                if (angle > s->config.maxTurnAngle) {
                    // 制限角度を超えている場合、baseDir から maxTurnAngle 分だけ回転させたベクトルにクランプ
                    // どちらに回転させるかを判断するために外積を使用
                    XMVECTOR cross = XMVector3Cross(baseDir, nextDir);
                    float sign = (XMVectorGetY(cross) >= 0) ? 1.0f : -1.0f;

                    // baseDir を Y軸周りに sign * maxTurnAngle 分回転させる
                    XMMATRIX rot = XMMatrixRotationY(sign * s->config.maxTurnAngle);
                    nextDir = XMVector3TransformNormal(baseDir, rot);
                }

                XMStoreFloat3(&s->dashDir, nextDir);
            }

            // --- 3. 速度の書き込み ---
            XMVECTOR finalVel = XMVectorScale(XMLoadFloat3(&s->dashDir), s->config.dashSpeed);
            XMStoreFloat3(&tps.externalVelocity, finalVel);
        });
}

REGISTER_LOGIC_SYSTEM(DodgeMovementSystem, Priority::LogicStage::L02_Update); // 状態更新の直後に実行