#include "PlayerMoveSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Game/Logics/Combat/DeadTag.h"
#include <SimpleMath.h>
#include <cmath>

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

void PlayerMoveSystem::Update(float dt)
{
    ForEachWithID([&](EntityID id,
        TransformComponent& trans,
        const PlayerComponent& players,
        CharacterMovementInputComponent& movementInput) {

            // 死亡している場合はペダルから足を離す（入力をゼロにする）
            if (_world->HasComponent<Tag::DeadTag>(id)) {
                movementInput.desiredVelocity = Vector3::Zero;
                movementInput.jumpRequested = false;
                return;
            }

            // =========================================================
            // 1. 移動ペダル（desiredVelocity）の決定
            // =========================================================
            Vector3 moveDir(players.input.moveDir.x, 0.0f, players.input.moveDir.z);

            // 斜め移動の時に速度が √2 倍にならないよう正規化
            if (moveDir.LengthSquared() > 0.001f) {
                moveDir.Normalize();
            }

            // プレイヤー固有の速度(moveSpeed)を掛けて、ペダル(InputComponent)に書き込む
            // ※実際の地形との衝突計算や移動は JoltCharacterUpdateSystem が勝手にやってくれます
            movementInput.desiredVelocity = moveDir * players.moveSpeed;
            movementInput.jumpRequested = players.input.jump;

            // =========================================================
            // 2. 振り向き（回転）の処理
            // =========================================================
            // 入力がある（移動しようとしている）場合のみ、その方向へ滑らかに振り向く
            if (moveDir.LengthSquared() > 0.001f) {
                float targetYaw = atan2f(moveDir.x, moveDir.z);
                Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);

                // Slerpで滑らかに回転 (turnSpeedで鋭さを調整)
                trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, players.turnSpeed * dt);

                // Joltへ不正な数値を送らないための絶対ルールの正規化
      		    DirectX::XMVector4Normalize(DirectX::XMLoadFloat4(&trans.rotation));
    
                // 回転したので行列再計算フラグを立てる
                trans.isStatic = false;
            }
        });
}

REGISTER_LOGIC_SYSTEM(PlayerMoveSystem, Priority::LogicStage::L02_Update);