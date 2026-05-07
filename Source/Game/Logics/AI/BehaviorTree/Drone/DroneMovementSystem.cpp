/**
 * @file DroneMovementSystem.cpp
 * @brief ドローン移動システムの実装
 */
#include "DroneMovementSystem.h"
#include "Game/Core/SystemPriority.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include <cmath>

using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

void DroneMovementSystem::Update(float dt) {

    ForEachParallel([dt](TransformComponent& trans, DroneComponent& drone) {

        drone.hoverTimeOffset += dt;

        Vector3 currentPos = trans.position;
        Vector3 targetPos = drone.targetPosition;

        switch (drone.currentState) {
        case DroneState::MoveToTarget: {
            // ========================================================
            // 滑らかな追従（Lerp）
            // ========================================================
            trans.position = Vector3::Lerp(trans.position, drone.targetPosition, 5.0f * dt);

            // 進行方向への滑らかな回転（Slerp）
            Vector3 dir = drone.targetPosition - trans.position;
            if (dir.LengthSquared() > 0.001f) {
                float targetYaw = atan2f(dir.x, dir.z);
                Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
                trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, 10.0f * dt);
                trans.isDirty = true; // Transformの更新を通知
            }
            break;
        }
        case DroneState::FireCharge: {
            // ========================================================
            // 目標座標への直線的かつ高速な突撃
            // ========================================================
            Vector3 dir = drone.targetPosition - trans.position;
            if (dir.LengthSquared() > 1.0f) {
                dir.Normalize();
                trans.position = trans.position + dir * drone.moveSpeed * dt;
                trans.isDirty = true;
            }
            break;
        }
        case DroneState::LockOn:
            // 発射直前のタメ状態（静止）
            break;
        case DroneState::Idle:
        {
            // ========================================================
            // 1. スプリング・ダンパー制御 (Hooke's Law + Damping)
            // ========================================================
            // 目標までの距離と方向
            Vector3 displacement = drone.targetPosition - trans.position;

            // パラメータ調整（インスペクタに出すとさらに良い）
            const float springTension = 50.0f; // ばねの強さ（引き戻す力）
            const float damping = 5.0f;        // ブレーキ（値が低いほどビヨンビヨン跳ねる）

            // 力の計算: F = -k*x - c*v
            Vector3 springForce = displacement * springTension;
            Vector3 dampingForce = drone.currentVelocity * damping;
            Vector3 acceleration = springForce - dampingForce;

            // 速度と座標の更新
            drone.currentVelocity += acceleration * dt;
            currentPos += drone.currentVelocity * dt;

            // ========================================================
            // 2. ホバーノイズの加算 (有機的な浮遊感)
            // ========================================================
            // Y軸（高さ）に対して、個体ごとに異なる周期のサイン波を加える
            float noiseY = std::sin(drone.hoverTimeOffset * 3.0f) * 0.15f;
            currentPos.y += noiseY;

            // 計算が終わった Vector3 を XMFLOAT3(trans.position) に戻す
            trans.position = currentPos;

            // ========================================================
            // 3. バンキング（進行方向への傾き計算）
            // ========================================================
            if (drone.currentVelocity.LengthSquared() > 0.01f) {
                // ベースとなるY軸の旋回（ターゲットの方向を向く）
                Vector3 dir = drone.targetPosition - trans.position;
                float targetYaw = atan2f(dir.x, dir.z);

                // 速度ベクトルから、前後（ピッチ）と左右（ロール）の傾きを計算
                // 速度のXZ成分を使って、ローカル空間での移動方向を割り出す
                Vector3 localVel = Vector3::TransformNormal(drone.currentVelocity, Matrix::CreateRotationY(-targetYaw));

                // 速度に応じた傾き角度（最大傾きを制限する）
                float maxTilt = DirectX::XMConvertToRadians(30.0f); // 最大30度
                float pitch = std::clamp(localVel.z * 0.05f, -maxTilt, maxTilt); // 前後移動でピッチ
                float roll = std::clamp(-localVel.x * 0.05f, -maxTilt, maxTilt); // 左右移動でロール

                // 最終的なクォータニオンの合成 (Yaw -> Pitch -> Roll)
                Quaternion targetRot = Quaternion::CreateFromYawPitchRoll(targetYaw, pitch, roll);

                // 回転も滑らかに補間する
                trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, 15.0f * dt);
            }

            trans.isDirty = true;

            break;
        }
        default:
            break;
        }
        });
}

// 実際の座標更新を行うため、L04_Physicsを指定
REGISTER_LOGIC_SYSTEM(DroneMovementSystem, Priority::LogicStage::L04_Physics);