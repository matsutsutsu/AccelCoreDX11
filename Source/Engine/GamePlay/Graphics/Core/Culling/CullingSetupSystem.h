#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/GamePlay/Graphics/Core/PrimitiveComponent.h"
#include "CullingComponents.h"

// Modelコンポーネントを持っているが、カリング用コンポーネントを持たないエンティティに
// 自動的に付与を行うシステム
class ModelCullingSetupSystem : public CCL::ECS::IfSystem<ModelCullingSetupSystem,
    CCL::ECS::Read<ModelComponent>>
{
public:
    ModelCullingSetupSystem();
    virtual ~ModelCullingSetupSystem() = default;

    void Update(float dt) override;
};

// Primitiveコンポーネントを持っているが、カリング用コンポーネントを持たないエンティティに
// 自動的に付与を行うシステム
class PrimitiveCullingSetupSystem : public CCL::ECS::IfSystem<PrimitiveCullingSetupSystem,
    CCL::ECS::Read<PrimitiveComponent>>
{
public:
    PrimitiveCullingSetupSystem();
    virtual ~PrimitiveCullingSetupSystem() = default;

    void Update(float dt) override;
};