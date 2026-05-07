#include "JoltCharacterUpdateSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include <Jolt/Physics/Body/BodyFilter.h>

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

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


void JoltCharacterUpdateSystem::Update(float dt) {
    if (!_world->HasResource<JoltPhysicsManager>())
      return;
    auto &joltManager = _world->GetResource<JoltPhysicsManager>();
    JPH::PhysicsSystem *physicsSystem = joltManager.GetPhysicsSystem();
    //JPH::TempAllocator *tempAllocator = joltManager.GetTempAllocator();
    JPH::BodyInterface &bodyInterface = joltManager.GetBodyInterface();

    JPH::TempAllocatorMalloc localTempAllocator;

    // キャラクターのアップデート設定（重力に対する姿勢制御など）
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;

    // 全コアを使って並列でキャラクターの衝突計算を行う
    ForEachWithID([&](CCL::ECS::EntityID id, TransformComponent &trans,
                              const JoltCharacterHandleComponent &handle,
                              const CharacterMovementInputComponent &input) {
    if (!handle.character) return;

    // =======================================================
    // プレイヤーの強制ワープ処理
    // =======================================================
    if (trans.isTeleported) {
      handle.character->SetPosition(
          JPH::RVec3(trans.position.x, trans.position.y, trans.position.z));
      handle.character->SetRotation(
          JPH::Quat(trans.rotation.x, trans.rotation.y, trans.rotation.z,
                    trans.rotation.w));

      // フラグを下ろす
      trans.isTeleported = false;
    }
 

    // =======================================================
    // 自分自身の分身（Kinematic剛体）を無視するフィルターを作成
    // =======================================================
    JPH::BodyID ignoreBodyID;
    if (auto *kinBody = _world->GetComponent<JoltHandleComponent>(id)) {
      ignoreBodyID = kinBody->bodyID;
    }
    JPH::IgnoreSingleBodyFilter bodyFilter(ignoreBodyID);

	// キャラクターの現在の速度を取得
    JPH::Vec3 currentVel = handle.character->GetLinearVelocity();

    // ペダル(input)の踏み込み量を見て、車(Jolt)を動かす
    handle.character->SetLinearVelocity(JPH::Vec3(
        input.desiredVelocity.x,
        currentVel.GetY(), // 重力は維持
        input.desiredVelocity.z
    ));


    // 1. 地形との衝突や段差の乗り越えを計算 (Jolt内部処理)
    handle.character->ExtendedUpdate(
        dt, physicsSystem->GetGravity(), updateSettings,
        physicsSystem->GetDefaultBroadPhaseLayerFilter(PhysicsLayers::MOVING),
        physicsSystem->GetDefaultLayerFilter(PhysicsLayers::MOVING),
        bodyFilter,
        {},
        localTempAllocator);


    // 2. 計算後の「位置」だけをECS側のTransformに同期する
    JPH::RVec3 pos = handle.character->GetPosition();
    trans.position = {pos.GetX(), pos.GetY(), pos.GetZ()};

    // ※回転（trans.rotation）は上書きしない！
    // Joltのカプセルは常に直立しており、振り向く処理はPlayerMoveSystem側で行うため。
    trans.isStatic = false; // 行列の再計算を要求

    // =======================================================
    // プレイヤーの「幽霊」を実体化させる同期処理
    // =======================================================
    if (auto* kinBody = _world->GetComponent<JoltHandleComponent>(id)) {
        JPH::Quat joltRot(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w);
        joltRot = joltRot.Normalized();
        bodyInterface.SetPositionAndRotation(kinBody->bodyID, pos, joltRot, JPH::EActivation::DontActivate);
    }


  });
}

// 物理のStepが走る前、または同じPhysicsフェーズで実行する
REGISTER_LOGIC_SYSTEM(JoltCharacterUpdateSystem, Priority::LogicStage::L03_PrePhysics);