#pragma once
#include "Engine/Graphics/Renderer/Pipeline/IRenderPass.h"

class PostProcessPass : public IRenderPass {
public:
    void Execute(const RenderContext& rc)override;
};