#pragma once
#include "Engine/Graphics/Renderer/Pipeline/IRenderPass.h"
#include <wrl.h>
#include <DirectXMath.h>

class SkyboxPass : public IRenderPass {
public:
    void Initialize(ID3D11Device* device) override;
    void Execute(const RenderContext& rc) override;
private:
    // 以下のメンバ変数を追加
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _skyboxTex;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _cbSkybox;

    struct CbSkybox {
        DirectX::XMFLOAT4X4 invViewProj;
    };
};