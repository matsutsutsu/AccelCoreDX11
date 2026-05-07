#pragma once
#include "IRenderPass.h"
#include "Engine/Graphics/Core/PipelineState.h"
#include "Engine/Graphics/Renderer/InstanceBuffer.h"

class TransparentPass : public IRenderPass {
public:
    void Initialize(ID3D11Device* device) override;
    void Execute(const RenderContext& rc)override;
private:
    PipelineState _psoTransparent;
    InstanceBuffer _instanceBuffer;
};