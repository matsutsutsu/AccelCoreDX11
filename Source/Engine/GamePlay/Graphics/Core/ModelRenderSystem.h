#pragma once
#include "ECS/System/CCL_System.h" // IfSystemの継承元

// テンプレート引数で使用するコンポーネント
#include "Engine/GamePlay/Graphics/Core/MaterialComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/GamePlay/Graphics/Core/Culling/CullingComponents.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

// 前方宣言
class ModelRenderer;

// 描画に必要なコンポーネント (TransformComponent, ModelComponent) をフィルタするシステム
class ModelRenderSystem : public CCL::ECS::IfSystem<ModelRenderSystem,
                              CCL::ECS::Read<TransformComponent>,
                              CCL::ECS::Read<ModelComponent>,
                              CCL::ECS::Read<MaterialComponent>,
                              CCL::ECS::Read<VisibilityComponent>>
{
  public:
    ModelRenderSystem();
    virtual ~ModelRenderSystem() = default;

    void Update(float dt) override;
};