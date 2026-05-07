#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "LambertShader.h"

LambertShader::LambertShader(ID3D11Device* device)
{
	// 頂点シェーダーのロード
	GpuResourceUtils::LoadVertexShader(
		device,
		"Assets/Shader/LambertVS.cso",
		ModelResource::InputElementDescs.data(),
		static_cast<UINT>(ModelResource::InputElementDescs.size()),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダーのロード
	GpuResourceUtils::LoadPixelShader(
		device,
		"Assets/Shader/LambertPS.cso",
		pixelShader.GetAddressOf());

	// 基底クラスの機能でリフレクション解析 & 定数バッファ自動生成
	InitializeReflection(device, "Assets/Shader/LambertPS.cso");

}

void LambertShader::Begin(const RenderContext& rc)
{
	// パイプラインの初期設定　バインドリセットなど
	ApplyShaderPipeline(rc.deviceContext, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void LambertShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// 1. マテリアル・プロパティの自動適用
	// これだけで "materialColor" のセットや "DiffuseMap" のバインドが完了します
	if (mesh.material && mesh.material->data)
	{
		UpdateMaterialProperties(dc, *mesh.material->data);
	}

}

void LambertShader::End(const RenderContext& rc)
{
	// シャドウマップsrvのスロット解除
	UnbindResources(rc.deviceContext);
}