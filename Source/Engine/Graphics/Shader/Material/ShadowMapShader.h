#pragma once
#include "Engine/Graphics/Shader/Material/Core/Shader.h"

class ShadowMapShader : public Shader {
public:
    ShadowMapShader(ID3D11Device* device);
    ~ShadowMapShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
};