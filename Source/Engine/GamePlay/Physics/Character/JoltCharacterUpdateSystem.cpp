#include "JoltCharacterUpdateSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include <Jolt/Physics/Body/BodyFilter.h>

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX;

// ===================================================================================
// 【 物理肉体更新システム : JoltCharacterUpdateSystem 】
//
// [ 役割 ]
// 「誰が運転しているか」を一切気にせず、入力された速度でキャラクターを地形に沿わせて動かす「運び屋」。
//
// [ ECSデータパイプライン ]
// 📥 READ  : CharacterMovementInputComponent (AIやプレイヤーが書き込んだ「希望する移動速度」)
// 📥 READ  : JoltCharacterHandleComponent    (Jolt側のキャラクター実体ID)
// 📤 WRITE : TransformComponent              (地形干渉を計算した後の、最終的なワールド座標)
//
// [ 内部挙動の直感的な解説 ]
// 脳（PlayerMoveSystem や NavSteeringSystem）が決定した「動きたい速度ベクトル」をペダルとして受け取り、
// Jolt Physics の仮想キャラクター（CharacterVirtual）のエンジンに流し込みます。
// 重力落下、壁ずり、段差の乗り越えといった複雑な物理計算はすべて Jolt 内部で完結し、
// システムの最後で計算された「正しい物理座標」だけを ECS の Transform に書き戻して画面に反映させます。
// ===================================================================================


void JoltCharacterUpdateSystem::Update(float rawDt) {
    if (!_world->HasResource<JoltPhysicsManager>()) return;

    auto& joltManager = _world->GetResource<JoltPhysicsManager>();
    JPH::PhysicsSystem* physicsSystem = joltManager.GetPhysicsSystem();
    JPH::BodyInterface& bodyInterface = joltManager.GetBodyInterface();

    JPH::TempAllocatorMalloc localTempAllocator;
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;

    ForEachWithID([&](CCL::ECS::EntityID id,
        TransformComponent& trans,
        const JoltCharacterHandleComponent& handle,
        const CharacterMovementInputComponent& input,
        const JoltCharacterConfigComponent& config,
        const TimeState& time)
        {
            if (!handle.character) return;

            // --- 1. テレポート処理 ---
            if (trans.isTeleported) {
                handle.character->SetPosition(JPH::RVec3(trans.position.x, trans.position.y, trans.position.z));
                handle.character->SetRotation(JPH::Quat(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w));
                trans.isTeleported = false;
            }

            // ===============================================================
            // --- 2. 速度と重力の適用（★ここが空中ダッシュの要！） ---
            // ===============================================================
            JPH::Vec3 currentVel = handle.character->GetLinearVelocity();
            float verticalVel = currentVel.GetY();

            // ★追加: 空中直線ダッシュフラグがONなら、重力を完全無視してY軸も上書き！
            if (input.overrideVerticalVelocity) {
                verticalVel = input.desiredVelocity.y;
            }
            else {
                // 通常の重力・ジャンプ処理
                if (handle.character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
                    verticalVel = 0.0f;

                    if (input.jumpRequested) {
                        // カスタムジャンプ力が指定されていれば優先
                        verticalVel = (input.customJumpVelocity > 0.01f) ? input.customJumpVelocity : config.jumpSpeed;
                    }
                }
                else {
                    // カスタム重力があればそれを使用 (Y軸は下向きなのでマイナスにする)
                    float currentGravityY = (input.customGravity > 0.01f) ? -input.customGravity : (physicsSystem->GetGravity().GetY() * config.gravityScale);
                    verticalVel += currentGravityY * time.localDt;
                }
            }

            // 最終的な速度を Jolt の実体にセット
            handle.character->SetLinearVelocity(JPH::Vec3(
                input.desiredVelocity.x,
                verticalVel,
                input.desiredVelocity.z
            ));

            // --- 4. 衝突計算と移動の実行 ---
            JPH::BodyID ignoreBodyID;
            if (auto* kinBody = _world->GetComponent<JoltHandleComponent>(id)) {
                ignoreBodyID = kinBody->bodyID;
            }
            JPH::IgnoreSingleBodyFilter bodyFilter(ignoreBodyID);

            // ★修正: CharacterVirtual内部の挙動計算にもカスタム重力を渡す
            JPH::Vec3 joltGravity = (input.customGravity > 0.01f) ? JPH::Vec3(0, -input.customGravity, 0) : (physicsSystem->GetGravity() * config.gravityScale);

            // =========================================================
            // ★追加: 物理計算が走る前に、ワープ（テレポート）要求を処理する
            // =========================================================
            if (trans.isTeleported) {
                // Joltの仮想キャラクターを、ECSで指定された座標へ強制移動させる
                handle.character->SetPosition(JPH::RVec3(trans.position.x, trans.position.y, trans.position.z));

                // 回転も同期させる場合
                JPH::Quat joltRot(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w);
                handle.character->SetRotation(joltRot);

                // ワープ完了。フラグを降ろして無限ループを防ぐ
                trans.isTeleported = false;
            }

            handle.character->ExtendedUpdate(
                time.localDt,
                joltGravity, // ← カスタム重力を渡す
                updateSettings,
                physicsSystem->GetDefaultBroadPhaseLayerFilter(PhysicsLayers::MOVING),
                physicsSystem->GetDefaultLayerFilter(PhysicsLayers::MOVING),
                bodyFilter,
                {},
                localTempAllocator);

            // --- 5. 結果をTransformへ同期 ---
            JPH::RVec3 pos = handle.character->GetPosition();
            trans.position = { pos.GetX(), pos.GetY(), pos.GetZ() };
            trans.isStatic = false;
            trans.isDirty = true;

            // 幽霊（Kinematic Body）の同期
            if (auto* kinBody = _world->GetComponent<JoltHandleComponent>(id)) {
                JPH::Quat joltRot(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w);
                bodyInterface.SetPositionAndRotation(kinBody->bodyID, pos, joltRot.Normalized(), JPH::EActivation::DontActivate);
            }
        });
}

// 物理のStepが走る前、または同じPhysicsフェーズで実行する
REGISTER_LOGIC_SYSTEM(JoltCharacterUpdateSystem, Priority::LogicStage::L03_PrePhysics);