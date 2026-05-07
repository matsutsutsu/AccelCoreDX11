#pragma once
#include "Engine/Graphics/Shader/Material/Core/Shader.h"

class OutlineShader : public Shader
{
public:
	OutlineShader(ID3D11Device* device);
	~OutlineShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
	void End(const RenderContext& rc) override;

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;

	// ラスタライザステートは固有の持ち物として維持
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
};