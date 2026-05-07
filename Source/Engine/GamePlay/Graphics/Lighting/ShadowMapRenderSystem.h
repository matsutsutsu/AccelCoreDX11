#pragma once

#include "ECS/System/CCL_System.h" // IfSystemの継承元

// テンプレート引数で使用するコンポーネント
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Graphics/Core/Culling/CullingComponents.h"

// 前方宣言
class ModelRenderer;
class Camera;
class LightManager;

// シャドウマップ専用の描画システム
class ShadowMapRenderSystem : public CCL::ECS::IfSystem<ShadowMapRenderSystem,
                                  CCL::ECS::Read<TransformComponent>,
                                  CCL::ECS::Read<ModelComponent>,
                                  CCL::ECS::Read<BoundingBoxComponent>>{

  public:
    ShadowMapRenderSystem();
    virtual ~ShadowMapRenderSystem() = default;

    void Update(float dt) override;

  private:
    void RenderShadowMap();
};