#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "ToonShader.h"
#include "Engine/Graphics/Shader/Pass/ShadowMap.h"

ToonShader::ToonShader(ID3D11Device* device)
{
	GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/ToonVS.cso",
		ModelResource::InputElementDescs.data(), (UINT)ModelResource::InputElementDescs.size(),
		inputLayout.GetAddressOf(), vertexShader.GetAddressOf());

	GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/ToonPS.cso", pixelShader.GetAddressOf());

	//GpuResourceUtils::LoadTexture(device, "Assets/Textures/Shader/ramp.png", rampTexture.GetAddressOf());

	// --- リフレクション初期化 ---
	// CbMesh と CbToon の両方を作成
	// LightとShadowMapは基底Shaderクラスの関数内で行っている
	if (InitializeReflection(device, "Assets/Shader/ToonPS.cso"))
	{
		// DiffuseMap や NormalMap のスロット取得は、UpdateMaterialProperties 内で
		// 自動的に行われるため、ここでの事前キャッシュは必須ではありません。
		// ただし、ランプテクスチャはマテリアルに含まれない（このクラス管理）のでキャッシュします。
		//_rampSlot = FetchTextureSlot("RampTexture");

	}
}

void ToonShader::Begin(const RenderContext& rc)
{
	// パイプラインの初期設定　バインドリセットなど
	ApplyShaderPipeline(rc.deviceContext, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void ToonShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// 1. マテリアル・プロパティの自動適用
		// Meshが持っている MaterialComponent の中身（色、数値、テクスチャ）を
		// リフレクションを使って対応する定数バッファ・スロットに流し込む
	if (mesh.material && mesh.material->data) // dataがMaterialComponentと仮定
	{
		UpdateMaterialProperties(dc, *mesh.material->data);
	}


	// 4. テクスチャのバインド

	//if (_rampSlot != -1) {
	//	dc->PSSetShaderResources((UINT)_rampSlot, 1, rampTexture.GetAddressOf());
	//}
}



void ToonShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シャドウマップsrvのスロット解除
	UnbindResources(dc);
}