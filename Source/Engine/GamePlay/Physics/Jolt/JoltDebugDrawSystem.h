#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltBoxColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltSphereColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltCapsuleColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltMeshColliderComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"

// 1. BoxCollider 可視化システム
class JoltBoxDebugDrawSystem : public CCL::ECS::IfSystem<JoltBoxDebugDrawSystem,
                                     CCL::ECS::Read<TransformComponent>,
                                     CCL::ECS::Read<JoltBoxColliderComponent>> {
public:
    JoltBoxDebugDrawSystem() : IfSystem("JoltBoxDebugDrawSystem") {
        this->isDebugVisible = true;
    }
    void Update(float dt) override;
};

// 2. SphereCollider 可視化システム
class JoltSphereDebugDrawSystem : public CCL::ECS::IfSystem<JoltSphereDebugDrawSystem,
                                         CCL::ECS::Read<TransformComponent>,
                                         CCL::ECS::Read<JoltSphereColliderComponent>> {
public:
    JoltSphereDebugDrawSystem() : IfSystem("JoltSphereDebugDrawSystem") {
        this->isDebugVisible = true;
    }
    void Update(float dt) override;
};

// 3. CapsuleCollider 可視化システム
class JoltCapsuleDebugDrawSystem : public CCL::ECS::IfSystem<JoltCapsuleDebugDrawSystem,
                                         CCL::ECS::Read<TransformComponent>,
                                         CCL::ECS::Read<JoltCapsuleColliderComponent>> {
public:
    JoltCapsuleDebugDrawSystem() : IfSystem("JoltCapsuleDebugDrawSystem") {
        this->isDebugVisible = true;
    }
    void Update(float dt) override;
};

// 4. MeshCollider 可視化システム
class JoltMeshDebugDrawSystem : public CCL::ECS::IfSystem<JoltMeshDebugDrawSystem,
                                         CCL::ECS::Read<TransformComponent>,
                                         CCL::ECS::Read<JoltMeshColliderComponent>,
                                         CCL::ECS::Read<ModelComponent>> {
public:
    JoltMeshDebugDrawSystem() : IfSystem("JoltMeshDebugDrawSystem") {
        //this->isDebugVisible = true;
    }
    void Update(float dt) override;
};