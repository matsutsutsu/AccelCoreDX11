// Engine/GamePlay/Physics/Jolt/JoltDebugRenderer.h
#pragma once
#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

class JoltDebugRenderer final : public JPH::DebugRendererSimple {
  private:
    ShapeRenderer *m_shapeRenderer;

  public:
    JoltDebugRenderer(ShapeRenderer *renderer) : m_shapeRenderer(renderer) {}

    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;


    virtual void DrawTriangle(JPH::RVec3Arg inV1,
        JPH::RVec3Arg                       inV2,
        JPH::RVec3Arg                       inV3,
        JPH::ColorArg                       inColor,
        JPH::DebugRenderer::ECastShadow     inCastShadow) override;

    virtual void DrawText3D(JPH::RVec3Arg inPosition,
        const JPH::string_view           &inString,
        JPH::ColorArg                     inColor,
        float                             inHeight) override
    {
    }
};