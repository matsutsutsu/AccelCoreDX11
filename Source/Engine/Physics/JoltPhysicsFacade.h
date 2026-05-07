#pragma once
#include "Engine/Physics/IPhysicsAPI.h"
#include "ECS/Core/CCL_World.h"

class JoltPhysicsFacade : public IPhysicsAPI {
private:
    CCL::ECS::Core::World* m_world;

public:
    JoltPhysicsFacade(CCL::ECS::Core::World* world) : m_world(world) {}
    virtual ~JoltPhysicsFacade() = default;

    // Linear
    void AddForce(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& force) override;
    void AddImpulse(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& impulse) override;
    void SetLinearVelocity(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& velocity) override;
    DirectX::XMFLOAT3 GetLinearVelocity(CCL::ECS::EntityID entity) override;

    // Angular
    void AddTorque(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& torque) override;
    void AddAngularImpulse(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& impulse) override;
    void SetAngularVelocity(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& velocity) override;
    DirectX::XMFLOAT3 GetAngularVelocity(CCL::ECS::EntityID entity) override;

    // Warp
    void SetPosition(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& position) override;
    void SetRotation(CCL::ECS::EntityID entity, const DirectX::XMFLOAT4& rotation) override;

    void SetLookDirection(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& direction) override;

    // Queries
    bool CheckGrounded(CCL::ECS::EntityID entity, float distance) override;
    PhysicsHitInfo RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, CCL::ECS::EntityID ignoreEntity) override;
};