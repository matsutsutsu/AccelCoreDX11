#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/GamePlay/Graphics/Core/PrimitiveComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "CullingComponents.h"

// ====================================================================
// Model用のAABB更新システム
// ====================================================================
class ModelBoundsUpdateSystem : public CCL::ECS::IfSystem<ModelBoundsUpdateSystem,
    CCL::ECS::Read<TransformComponent>,
    CCL::ECS::Read<ModelComponent>,
    CCL::ECS::Write<BoundingBoxComponent>>
{
public:
    ModelBoundsUpdateSystem();
    virtual ~ModelBoundsUpdateSystem() = default;

    void Update(float dt) override;
};

// ====================================================================
// Primitive用のAABB更新システム
// ====================================================================
class PrimitiveBoundsUpdateSystem : public CCL::ECS::IfSystem<PrimitiveBoundsUpdateSystem,
    CCL::ECS::Read<TransformComponent>,
    CCL::ECS::Read<PrimitiveComponent>,
    CCL::ECS::Write<BoundingBoxComponent>>
{
public:
    PrimitiveBoundsUpdateSystem();
    virtual ~PrimitiveBoundsUpdateSystem() = default;

    void Update(float dt) override;
};