#include "NavSteeringSystem.h"
#include "ECS/Core/CCL_World.h"
#include "SimpleMath.h"
#include <algorithm>

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX::SimpleMath;
using namespace CCL::ECS;

// ===================================================================================
// 【 ナビゲーション (操舵) : NavSteeringSystem 】
//
// [ 役割 ]
// 計算された道筋（Waypoints）を辿り、物理エンジンへ渡すための移動ベクトルを作る「操舵手」。
//
// [ 実行順序 ]
// L02_AI_Act (230) : 経路が確定し、速度指示が出た後に、具体的な移動ベクトルを算出する。
//
// [ ECSデータパイプライン ]
// 📤 WRITE : TransformComponent (現在地)
// 📥 WRITE : NavAgentComponent  (waypoints, currentDesiredSpeed)
// 📤 WRITE : CharacterMovementInputComponent (物理エンジンへの入力速度)
// 📤 WRITE : NavAgentComponent  (currentWaypointIndex の更新)
// ===================================================================================

void NavSteeringSystem::Update(float dt) {

    ForEachWithID([&](EntityID entity,
        TransformComponent& transform,
        NavAgentComponent& navAgent,
        CharacterMovementInputComponent& input) {

            // 1. パスがない、または到着済みの場合は停止命令を出す
            if (navAgent.waypoints.empty() || navAgent.currentWaypointIndex >= navAgent.waypoints.size()) {
                input.desiredVelocity = Vector3::Zero;
                return;
            }

            Vector3 currentPos = transform.position;
            Vector3 targetPos = navAgent.waypoints[navAgent.currentWaypointIndex];

            // 2. 水平方向へのベクトル計算（ガタつき防止の鉄則）
            Vector3 dirToTarget = targetPos - currentPos;
            dirToTarget.y = 0.0f;

            float distToTarget = dirToTarget.Length();

            // 到着判定は PathfindingSystem 側でやるのが理想ですが、一旦ここで処理
            if (distToTarget <= navAgent.stopDistance) {
                // 次の目印へターゲットを切り替える
                navAgent.currentWaypointIndex++;

                // 切り替えた直後なので、このフレームはいったん処理を終え、
                // 次のフレームで新しい目標に向かって計算を再開させる
                return;
            }

            // 3. AI（外部）が指定した速度を取得
            float speed = navAgent.currentDesiredSpeed;

            // 4. キャラクターコントローラーへの「最終入力ベクトル」を生成
            if (distToTarget > 0.001f && speed > 0.0f) {
                dirToTarget.Normalize();
                input.desiredVelocity = dirToTarget * speed;
                input.desiredLookDir = dirToTarget;

                // AIの視線を滑らかに回転させる (Y軸回転)
                // atan2f を使って、XとZのベクトルから「向くべき角度(Yaw)」を計算
                float targetYaw = atan2f(dirToTarget.x, dirToTarget.z);
                Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);


                // ★フレーム落ちした際に係数が1.0を超えて逆回転するのを防ぐ
                float slerpAmount = (std::min)(10.0f * dt, 1.0f);
                transform.rotation = Quaternion::Slerp(transform.rotation, targetRot, slerpAmount);


            }

            else {
                input.desiredVelocity = Vector3::Zero;
            }
        });
}

REGISTER_LOGIC_SYSTEM(NavSteeringSystem, Priority::LogicStage::L02_AI_Act)