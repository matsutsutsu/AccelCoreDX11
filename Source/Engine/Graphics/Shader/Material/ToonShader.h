#pragma once
#include "Engine/Graphics/Shader/Material/Core/Shader.h"

class ToonShader : public Shader
{
public:
	ToonShader(ID3D11Device* device);
	~ToonShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
	void End(const RenderContext& rc) override;

	//// DrawGui はマテリアルエディタへ役割を譲るため削除、
	//// もしくは「シェーダー固有の設定（ランプテクスチャなど）」のみを表示するようにします
	//void DrawGui() override;

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;

};