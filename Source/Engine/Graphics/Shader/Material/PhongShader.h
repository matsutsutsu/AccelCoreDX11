#pragma once
#include "Engine/Graphics/Shader/Material/Core/Shader.h"

class PhongShader : public Shader
{
public:
	PhongShader(ID3D11Device* device);
	~PhongShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
	void End(const RenderContext& rc) override;


	void CheckAndReload(ID3D11Device *device) override; 

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;

	std::string  _hlslPathStr;
    std::wstring _hlslPathWStr;
};