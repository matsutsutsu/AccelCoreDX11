#include "Engine/Physics/JoltPhysicsFacade.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyFilter.h>

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

using namespace DirectX;

// =======================================================
// 🔵 Linear (直進)
// =======================================================
void JoltPhysicsFacade::AddForce(CCL::ECS::EntityID entity, const XMFLOAT3& force) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    bodyInterface.AddForce(handle->bodyID, JPH::Vec3(force.x, force.y, force.z));
    bodyInterface.ActivateBody(handle->bodyID);
}

void JoltPhysicsFacade::AddImpulse(CCL::ECS::EntityID entity, const XMFLOAT3& impulse) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    bodyInterface.AddImpulse(handle->bodyID, JPH::Vec3(impulse.x, impulse.y, impulse.z));
    bodyInterface.ActivateBody(handle->bodyID);
}

void JoltPhysicsFacade::SetLinearVelocity(CCL::ECS::EntityID entity, const XMFLOAT3& velocity) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    bodyInterface.SetLinearVelocity(handle->bodyID, JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

XMFLOAT3 JoltPhysicsFacade::GetLinearVelocity(CCL::ECS::EntityID entity) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return XMFLOAT3(0, 0, 0);
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    JPH::Vec3 vel = bodyInterface.GetLinearVelocity(handle->bodyID);
    return XMFLOAT3(vel.GetX(), vel.GetY(), vel.GetZ());
}

// =======================================================
// 🟢 Angular (回転)
// =======================================================
void JoltPhysicsFacade::AddTorque(CCL::ECS::EntityID entity, const XMFLOAT3& torque) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    bodyInterface.AddTorque(handle->bodyID, JPH::Vec3(torque.x, torque.y, torque.z));
    bodyInterface.ActivateBody(handle->bodyID);
}

void JoltPhysicsFacade::AddAngularImpulse(CCL::ECS::EntityID entity, const XMFLOAT3& impulse) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    bodyInterface.AddAngularImpulse(handle->bodyID, JPH::Vec3(impulse.x, impulse.y, impulse.z));
    bodyInterface.ActivateBody(handle->bodyID);
}

void JoltPhysicsFacade::SetAngularVelocity(CCL::ECS::EntityID entity, const XMFLOAT3& velocity) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    bodyInterface.SetAngularVelocity(handle->bodyID, JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

XMFLOAT3 JoltPhysicsFacade::GetAngularVelocity(CCL::ECS::EntityID entity) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return XMFLOAT3(0, 0, 0);
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();
    JPH::Vec3 vel = bodyInterface.GetAngularVelocity(handle->bodyID);
    return XMFLOAT3(vel.GetX(), vel.GetY(), vel.GetZ());
}

// =======================================================
// 🟡 Kinematic Warp (強制制御)
// =======================================================
void JoltPhysicsFacade::SetPosition(CCL::ECS::EntityID entity, const XMFLOAT3& position) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    bodyInterface.SetPosition(handle->bodyID, JPH::RVec3(position.x, position.y, position.z), JPH::EActivation::Activate);
}

void JoltPhysicsFacade::SetRotation(CCL::ECS::EntityID entity, const XMFLOAT4& rotation) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    bodyInterface.SetRotation(handle->bodyID, JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), JPH::EActivation::Activate);
}

void JoltPhysicsFacade::SetLookDirection(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& direction) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    if (!handle) return;

    // 物理エンジンのインターフェースを取得 (※既存の定義に合わせてください)
    JPH::BodyInterface& bodyInterface = m_world->GetResource<JoltPhysicsManager>().GetBodyInterface();

    // ========================================================================
    // 1. 水平化 (Y軸の無効化)
    // 目的地が上や下にあっても、Y成分を強制的に 0 にすることで「Yaw(左右)」だけの回転にする
    // ========================================================================
    JPH::Vec3 flatDirection(direction.x, 0.0f, direction.z);

    // ========================================================================
    // 2. デッドゾーンのチェック (到着時の荒ぶる回転を防止)
    // ベクトルの長さ（の2乗）が極小の場合は、すでに到着しているとみなして回転しない
    // ========================================================================
    if (flatDirection.LengthSq() > 1e-4f) {

        flatDirection = flatDirection.Normalized();

        // 3. 基準の前方ベクトル（+Z）から目標方向へのクォータニオンを生成
        // ※もしあなたのモデルやエンジンの前方が「-Z」の場合は JPH::Vec3(0, 0, -1) にしてください
        JPH::Quat targetRotation = JPH::Quat::sFromTo(JPH::Vec3(0.0f, 0.0f, 1.0f), flatDirection);

        // 4. Joltの剛体に回転を適用し、スリープ状態なら叩き起こす(Activate)
        bodyInterface.SetRotation(handle->bodyID, targetRotation, JPH::EActivation::Activate);
    }
}

// =======================================================
// 🔴 Queries (レイキャスト)
// =======================================================
bool JoltPhysicsFacade::CheckGrounded(CCL::ECS::EntityID entity, float distance) {
    auto* handle = m_world->GetComponent<JoltHandleComponent>(entity);
    auto* trans = m_world->GetComponent<TransformComponent>(entity);
    if (!handle || !trans) return false;

    auto& physicsManager = m_world->GetResource<JoltPhysicsManager>();
    const JPH::NarrowPhaseQuery& narrowPhase = physicsManager.GetPhysicsSystem()->GetNarrowPhaseQuery();

    JPH::RVec3 rayStart(trans->position.x, trans->position.y, trans->position.z);
    JPH::RVec3 rayDir(0.0f, -distance, 0.0f);
    JPH::RayCastResult hit;
    JPH::IgnoreSingleBodyFilter ignoreSelf(handle->bodyID);

    return narrowPhase.CastRay(
        JPH::RRayCast(rayStart, rayDir), hit,
        JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter(), ignoreSelf
    );
}

PhysicsHitInfo JoltPhysicsFacade::RayCast(const XMFLOAT3& origin, const XMFLOAT3& direction, float maxDistance, CCL::ECS::EntityID ignoreEntity) {
    PhysicsHitInfo info;
    auto& physicsManager = m_world->GetResource<JoltPhysicsManager>();
    const JPH::NarrowPhaseQuery& narrowPhase = physicsManager.GetPhysicsSystem()->GetNarrowPhaseQuery();

    JPH::RVec3 rayStart(origin.x, origin.y, origin.z);

    XMVECTOR dirVec = XMVector3Normalize(XMLoadFloat3(&direction));
    dirVec = XMVectorScale(dirVec, maxDistance);
    XMFLOAT3 finalDir;
    XMStoreFloat3(&finalDir, dirVec);
    JPH::RVec3 rayDir(finalDir.x, finalDir.y, finalDir.z);

    JPH::BodyID ignoreBodyID;
    if (ignoreEntity != CCL::ECS::InvalidEntityID) {
        if (auto* handle = m_world->GetComponent<JoltHandleComponent>(ignoreEntity)) {
            ignoreBodyID = handle->bodyID;
        }
    }
    JPH::IgnoreSingleBodyFilter ignoreFilter(ignoreBodyID);

    JPH::RayCastResult hit;
    bool isHit = narrowPhase.CastRay(
        JPH::RRayCast(rayStart, rayDir), hit,
        JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter(), ignoreFilter
    );

    if (isHit) {
        info.hit = true;
        info.distance = maxDistance * hit.mFraction;

        JPH::RVec3 hitPos = rayStart + rayDir * hit.mFraction;
        info.position = { hitPos.GetX(), hitPos.GetY(), hitPos.GetZ() };

        const JPH::BodyLockInterface& lockInterface = physicsManager.GetPhysicsSystem()->GetBodyLockInterface();
        JPH::BodyLockRead lock(lockInterface, hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            info.entity = static_cast<CCL::ECS::EntityID>(body.GetUserData());

            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPos);
            info.normal = { normal.GetX(), normal.GetY(), normal.GetZ() };
        }
    }
    return info;
}