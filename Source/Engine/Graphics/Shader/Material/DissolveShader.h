#pragma once
#include "Engine/Graphics/Shader/Material/Core/Shader.h"

class DissolveShader : public Shader {
  public:
    DissolveShader(ID3D11Device *device);
    ~DissolveShader() override = default;

    void Begin(const RenderContext &rc) override;
    void Update(const RenderContext &rc, const Model::Mesh &mesh) override;
    void End(const RenderContext &rc) override;

  private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
};