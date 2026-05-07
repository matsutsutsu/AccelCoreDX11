#pragma once
#include "Engine/Graphics/Shader/Material/Core/Shader.h"

class PBRShader : public Shader {
  public:
    PBRShader(ID3D11Device *device);
    ~PBRShader() override = default;

    // 基底クラスの仮想関数をオーバーライド
    void Begin(const RenderContext &rc) override;
    void Update(const RenderContext &rc, const Model::Mesh &mesh) override;
    void End(const RenderContext &rc) override;

  private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;

};