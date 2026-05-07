#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltBoxColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltSphereColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltCapsuleColliderComponent.h"

// ===================================================================================
// 実行時にコライダーの形状変更(isDirty)を検知し、Joltの剛体に「外科手術」を行うシステム群。
// パフォーマンスを最大化するため、GetComponentを排除し、形状ごとにシステムを分割しています。
// ===================================================================================

// 1. BoxCollider 専用更新システム
class JoltBoxShapeUpdateSystem : public CCL::ECS::IfSystem<JoltBoxShapeUpdateSystem,
                                     CCL::ECS::Read<JoltHandleComponent>,
                                     CCL::ECS::Write<JoltBoxColliderComponent>> {
public:
    JoltBoxShapeUpdateSystem() : IfSystem("JoltBoxShapeUpdateSystem") {}
    void Update(float dt) override;
};

// 2. SphereCollider 専用更新システム
class JoltSphereShapeUpdateSystem : public CCL::ECS::IfSystem<JoltSphereShapeUpdateSystem,
                                        CCL::ECS::Read<JoltHandleComponent>,
                                        CCL::ECS::Write<JoltSphereColliderComponent>> {
public:
    JoltSphereShapeUpdateSystem() : IfSystem("JoltSphereShapeUpdateSystem") {}
    void Update(float dt) override;
};

// 3. CapsuleCollider 専用更新システム
class JoltCapsuleShapeUpdateSystem : public CCL::ECS::IfSystem<JoltCapsuleShapeUpdateSystem,
                                         CCL::ECS::Read<JoltHandleComponent>,
                                         CCL::ECS::Write<JoltCapsuleColliderComponent>> {
public:
    JoltCapsuleShapeUpdateSystem() : IfSystem("JoltCapsuleShapeUpdateSystem") {}
    void Update(float dt) override;
};