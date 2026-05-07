#include "JoltCharacterSetupSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"

#include "Engine/GamePlay/Physics/Collision/JoltCapsuleColliderComponent.h" // カプセルコライダーを使用前提
#include "JoltCharacterHandleComponent.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void JoltCharacterSetupSystem::Update(float dt)
{
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    auto               &joltManager   = _world->GetResource<JoltPhysicsManager>();
    JPH::PhysicsSystem *physicsSystem = joltManager.GetPhysicsSystem();

    ForEachWithID([&](CCL::ECS::EntityID                  id,
                      const TransformComponent           &trans,
                      const JoltCharacterConfigComponent &config) {
        // すでに生成済みならスキップ
        if (_world->HasComponent<JoltCharacterHandleComponent>(id)) {
            return;
        }

        // キャラクターは基本的にカプセル形状で作る（段差を登りやすくするため）
        auto *capsule = _world->GetComponent<JoltCapsuleColliderComponent>(id);
        if (!capsule) return; // カプセルが無ければ作れない

        // 1. カプセル形状の作成
        JPH::RefConst<JPH::Shape> characterShape;
        // 1. カプセル形状の作成 (必ずnewでヒープ確保し、RefConstで受ける)
        JPH::RefConst<JPH::ShapeSettings> capsuleSettings =
            new JPH::CapsuleShapeSettings(capsule->halfHeight, capsule->radius);


        // オフセットをかける設定も同様にnewで確保する
        // カプセルの中心を足元から半分の高さにオフセットするための設定
        JPH::RefConst<JPH::ShapeSettings> rotTransSettings =
            new JPH::RotatedTranslatedShapeSettings(
                JPH::Vec3(0, capsule->halfHeight + capsule->radius, 0),
                JPH::Quat::sIdentity(),
                capsuleSettings // アドレス(&)ではなく、スマートポインタをそのまま渡す
            );

        // 最終的なShapeの生成
        characterShape = rotTransSettings->Create().Get();

        // 2. CharacterVirtual の設定
        // キャラクター専用の設定群
        JPH::CharacterVirtualSettings settings;
        settings.mShape = characterShape;

        // キャラクターの足元を判定する平面センサーを設定
        settings.mSupportingVolume =
            JPH::Plane(JPH::Vec3::sAxisY(), -capsule->radius); // 足元の判定面
        settings.mMaxSlopeAngle             = 
            JPH::DegreesToRadians(config.maxSlopeAngle); // 登れる最大角度をラジアンに変換して設定
        settings.mMaxStrength               = config.characterMass * 10.0f; // 他の物体を押す力
        settings.mCharacterPadding          = 0.02f; // 壁への張り付きを防ぐ微小な隙間
        settings.mPenetrationRecoverySpeed  = 1.0f;
        settings.mPredictiveContactDistance = 0.1f;

        // 3. 実体の生成
        auto character = new JPH::CharacterVirtual(&settings,
            JPH::RVec3(trans.position.x, trans.position.y, trans.position.z),
            JPH::Quat(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w),
            physicsSystem);

        // 4. ECSに実体を登録
        JoltCharacterHandleComponent handleComp;
        handleComp.character = character;
        _world->AddComponent<JoltCharacterHandleComponent>(id, handleComp);
    });
}

// 物理実体が生成される前に呼ばれるようにする
REGISTER_LOGIC_SYSTEM(JoltCharacterSetupSystem, Priority::LogicStage::L03_PrePhysics);