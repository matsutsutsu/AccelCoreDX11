#include "JoltShapeUpdateSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h> // ★必須

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <DirectXMath.h> // 角度変換用

using namespace DirectX;

// ヘルパー：Degree(度)から JoltのQuaternion への変換
inline JPH::Quat EulerDegreeToJoltQuat(const XMFLOAT3& eulerDegree) {
    float pitch = XMConvertToRadians(eulerDegree.x);
    float yaw   = XMConvertToRadians(eulerDegree.y);
    float roll  = XMConvertToRadians(eulerDegree.z);
    // Jolt は ZYX 順（Yaw, Pitch, Roll）で回転を合成する仕様
    return JPH::Quat::sEulerAngles(JPH::Vec3(pitch, yaw, roll));
}

// ===================================================================================
// 1. BoxCollider の更新
// ===================================================================================
void JoltBoxShapeUpdateSystem::Update(float dt) {
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    JPH::BodyInterface &bodyInterface = _world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    ForEachWithID([&](CCL::ECS::EntityID id, const JoltHandleComponent &handle, JoltBoxColliderComponent &box) {
        if (!box.isDirty) return; 

        // 1. ベースの箱を作成
        JPH::BoxShapeSettings baseSettings(JPH::Vec3(box.halfExtent.x, box.halfExtent.y, box.halfExtent.z));
        JPH::ShapeRefC baseShape = baseSettings.Create().Get();

        // 2. オフセットと回転を適用したラッパー(包み紙)を作成！
        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(box.localOffset.x, box.localOffset.y, box.localOffset.z),
            EulerDegreeToJoltQuat(box.localRotationEuler),
            baseShape
        );

        bodyInterface.SetShape(handle.bodyID, offsetSettings.Create().Get(), true, JPH::EActivation::Activate);
        box.isDirty = false;
    });
}

// ===================================================================================
// 2. SphereCollider の更新
// ===================================================================================
void JoltSphereShapeUpdateSystem::Update(float dt) {
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    JPH::BodyInterface &bodyInterface = _world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    ForEachWithID([&](CCL::ECS::EntityID id, const JoltHandleComponent &handle, JoltSphereColliderComponent &sphere) {
        if (!sphere.isDirty) return; 

        JPH::SphereShapeSettings baseSettings(sphere.radius);
        JPH::ShapeRefC baseShape = baseSettings.Create().Get();

        // 球は回転させても意味がないため、位置ズレのみ適用
        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(sphere.localOffset.x, sphere.localOffset.y, sphere.localOffset.z),
            JPH::Quat::sIdentity(),
            baseShape
        );

        bodyInterface.SetShape(handle.bodyID, offsetSettings.Create().Get(), true, JPH::EActivation::Activate);
        sphere.isDirty = false;
    });
}

// ===================================================================================
// 3. CapsuleCollider の更新
// ===================================================================================
void JoltCapsuleShapeUpdateSystem::Update(float dt) {
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    JPH::BodyInterface &bodyInterface = _world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    ForEachWithID([&](CCL::ECS::EntityID id, const JoltHandleComponent &handle, JoltCapsuleColliderComponent &capsule) {
        if (!capsule.isDirty) return; 

        JPH::CapsuleShapeSettings baseSettings(capsule.halfHeight, capsule.radius);
        JPH::ShapeRefC baseShape = baseSettings.Create().Get();

        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(capsule.localOffset.x, capsule.localOffset.y, capsule.localOffset.z),
            EulerDegreeToJoltQuat(capsule.localRotationEuler),
            baseShape
        );

        bodyInterface.SetShape(handle.bodyID, offsetSettings.Create().Get(), true, JPH::EActivation::Activate);
        capsule.isDirty = false;
    });
}

REGISTER_LOGIC_SYSTEM(JoltBoxShapeUpdateSystem,     Priority::LogicStage::L03_PrePhysics);
REGISTER_LOGIC_SYSTEM(JoltSphereShapeUpdateSystem,  Priority::LogicStage::L03_PrePhysics);
REGISTER_LOGIC_SYSTEM(JoltCapsuleShapeUpdateSystem, Priority::LogicStage::L03_PrePhysics);