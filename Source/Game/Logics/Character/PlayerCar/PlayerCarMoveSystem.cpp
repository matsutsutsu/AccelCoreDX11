#include "PlayerCarMoveSystem.h"
#include "ECS/Core/CCL_World.h"
#include <DirectXMath.h>
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// ★ Joltのインクルードは一切不要！純粋なAPI窓口だけを呼ぶ
#include "Engine/Physics/IPhysicsAPI.h" 
#include <memory>

using namespace DirectX;
using namespace CCL::ECS;

PlayerCarMoveSystem::PlayerCarMoveSystem() : IfSystem("PlayerCarMoveSystem") {}

void PlayerCarMoveSystem::Update(float dt) {
    // 1. スマートポインタ経由でFacadeを取得
    if (!_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;
    auto physics = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();

    ForEach([&](TransformComponent& visualTrans, PlayerCarComponent& car) {

        // --- 初回回転とハンドル操作 ---
        if (!car.isLogicRotInitialized) {
            car.logicRotation = visualTrans.rotation;
            car.isLogicRotInitialized = true;
        }

        XMVECTOR logicRot = XMLoadFloat4(&car.logicRotation);
        if (car.input.steering != 0.0f) {
            float turnRadian = XMConvertToRadians(car.input.steering * car.turnSpeed * dt);
            XMVECTOR turnRot = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), turnRadian);
            logicRot = XMQuaternionMultiply(logicRot, turnRot);
            XMStoreFloat4(&car.logicRotation, logicRot);
        }

        // --- 物理球体との同期 ---
        if (!_world->IsEntityValid(car.physicsSphereID)) return;
        auto* sphereTrans = _world->GetComponent<TransformComponent>(car.physicsSphereID);
        if (!sphereTrans) return; // ★ JoltHandleComponent の取得すら不要に！

        // =======================================================
        // ★ Facadeを使った「接地判定(IsGrounded)」
        // =======================================================
        // 引数には JoltのID ではなく、EntityID を渡すだけ！
        car.isGrounded = physics->CheckGrounded(car.physicsSphereID, car.raycastLength);

        // --- フェイク・サスペンション ---
        float targetPitch = 0.0f;
        float targetRoll = 0.0f;

        if (car.isGrounded) {
            targetPitch = car.input.throttle * -car.maxPitchAngle;
            targetRoll = car.input.steering * -car.maxRollAngle;
        }

        car.currentPitch += (targetPitch - car.currentPitch) * car.suspensionSpeed * dt;
        car.currentRoll += (targetRoll - car.currentRoll) * car.suspensionSpeed * dt;

        // --- 描画位置と回転の計算 ---
        XMVECTOR offsetVec = XMLoadFloat3(&car.visualOffset);
        offsetVec = XMVector3Rotate(offsetVec, logicRot);
        XMVECTOR spherePos = XMLoadFloat3(&sphereTrans->position);
        XMStoreFloat3(&visualTrans.position, XMVectorAdd(spherePos, offsetVec));

        XMVECTOR offsetRot = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(car.visualRotation.x + car.currentPitch),
            XMConvertToRadians(car.visualRotation.y),
            XMConvertToRadians(car.visualRotation.z + car.currentRoll)
        );
        XMVECTOR finalRot = XMQuaternionMultiply(offsetRot, logicRot);
        XMStoreFloat4(&visualTrans.rotation, finalRot);

        // --- 車の向きベクトル計算 ---
        XMVECTOR forwardVec = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), logicRot);
        forwardVec = XMVectorSetY(forwardVec, 0.0f);
        forwardVec = XMVector3Normalize(forwardVec);

        XMVECTOR rightVec = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), logicRot);
        rightVec = XMVectorSetY(rightVec, 0.0f);
        rightVec = XMVector3Normalize(rightVec);

        // =======================================================
        // ★ Facadeを使った物理操作（Joltのコードが完全消滅！）
        // =======================================================
        if (car.isGrounded) {

            // 1. アクセル
            if (car.input.throttle != 0.0f) {
                XMFLOAT3 forward;
                XMStoreFloat3(&forward, forwardVec);
                float finalForce = car.acceleration * 100.0f;

                // JPH::Vec3 ではなく XMFLOAT3 を作成し、EntityID と共に渡す！
                XMFLOAT3 force(forward.x * car.input.throttle * finalForce, 0.0f, forward.z * car.input.throttle * finalForce);
                physics->AddForce(car.physicsSphereID, force);
                // ※ ActivateBody は Facade の中で行われるため不要
            }

            // 2. ドリフトキャンセラー
            // JPH::Vec3 ではなく XMFLOAT3 で現在の速度を受け取る！
            XMFLOAT3 currentVel = physics->GetLinearVelocity(car.physicsSphereID);
            XMVECTOR currentVelVec = XMLoadFloat3(&currentVel);

            float rightSpeed = XMVectorGetX(XMVector3Dot(currentVelVec, rightVec));
            XMVECTOR cancelVel = XMVectorScale(rightVec, -rightSpeed * 0.95f);

            XMFLOAT3 newVel(
                currentVel.x + XMVectorGetX(cancelVel),
                currentVel.y,
                currentVel.z + XMVectorGetZ(cancelVel)
            );

            // XMFLOAT3 を渡して速度を上書き！
            physics->SetLinearVelocity(car.physicsSphereID, newVel);
        }
        });
}

REGISTER_LOGIC_SYSTEM(PlayerCarMoveSystem, Priority::LogicStage::L02_Update);