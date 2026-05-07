#pragma once
#include "Engine/Graphics/Renderer/Pipeline/IRenderPass.h"

class ParticlePass : public IRenderPass {
public:
    void Execute(const RenderContext& rc)override;
};