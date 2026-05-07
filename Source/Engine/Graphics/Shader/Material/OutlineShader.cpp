#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "OutlineShader.h"
#include "Engine/Graphics/Core/Graphics.h"

OutlineShader::OutlineShader(ID3D11Device* device)
{
	// シェーダーロード
	GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/OutlineVS.cso",
		ModelResource::InputElementDescs.data(), (UINT)ModelResource::InputElementDescs.size(),
		inputLayout.GetAddressOf(), vertexShader.GetAddressOf());

	GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/OutlinePS.cso", pixelShader.GetAddressOf());

	// リフレクション初期化：CbMesh と CbOutline の両方を作成
	InitializeReflection(device, "Assets/Shader/OutlineVS.cso");

	// ラスタライザステート（表面カリング）の作成
	D3D11_RASTERIZER_DESC rsDesc = {};
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_FRONT; // アウトラインなので前を消す
	rsDesc.DepthClipEnable = TRUE;
	device->CreateRasterizerState(&rsDesc, m_rasterizerState.GetAddressOf());
}

void OutlineShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// パイプラインの初期設定　バインドリセットなど
	ApplyShaderPipeline(dc, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());

	// ラスタライザ適用
	dc->RSSetState(m_rasterizerState.Get());

}

void OutlineShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// 1. マテリアル・プロパティの自動適用
	// これにより、"materialColor" (CbMesh) や "color", "size" (CbOutline) が自動セットされます
	if (mesh.material && mesh.material->data)
	{
		UpdateMaterialProperties(dc, *mesh.material->data);
	}

	// もし CbOutline 内の変数がマテリアルに含まれていない場合、
	// 0 が送られる可能性があるため、必要ならここでデフォルト値をセットする手もあるが、
	// 基本的には「マテリアルにデータを入れておく」運用にする。
}

void OutlineShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// ラスタライザをデフォルト（裏面カリング）に戻す
	dc->RSSetState(Graphics::Instance().GetRenderState()->GetRasterizerState(RasterizerState::SolidCullBack));

	// スロット解除
	UnbindResources(dc);
}

