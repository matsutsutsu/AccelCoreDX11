#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "BasicShader.h"

BasicShader::BasicShader(ID3D11Device* device)
{
	// 1. シェーダーバイナリのロード
	GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/BasicVS.cso",
		ModelResource::InputElementDescs.data(), (UINT)ModelResource::InputElementDescs.size(),
		inputLayout.GetAddressOf(), vertexShader.GetAddressOf());

	GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/BasicPS.cso", pixelShader.GetAddressOf());

	// 2. 基底クラスの機能でリフレクション解析 & CbMeshバッファの自動生成
	if (InitializeReflection(device, "Assets/Shader/BasicPS.cso"))
	{

	}
}

void BasicShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// パイプラインの初期設定　バインドリセットなど
	ApplyShaderPipeline(dc, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void BasicShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// 1. マテリアル・プロパティの自動適用
		// Meshが持っている MaterialComponent の中身（色、数値、テクスチャ）を
		// リフレクションを使って対応する定数バッファ・スロットに流し込む
	if (mesh.material && mesh.material->data) // dataがMaterialComponentと仮定
	{
		UpdateMaterialProperties(dc, *mesh.material->data);
	}

}

void BasicShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シャドウマップsrvのスロット解除
	UnbindResources(dc);
}