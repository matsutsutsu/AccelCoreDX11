#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "PhongShader.h"
#include "Engine/Graphics/Shader/Pass/ShadowMap.h"
#include <windows.h>

PhongShader::PhongShader(ID3D11Device* device)
{
	// 頂点シェーダーのロード
	GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/PhongVS.cso",
		ModelResource::InputElementDescs.data(), (UINT)ModelResource::InputElementDescs.size(),
		inputLayout.GetAddressOf(), vertexShader.GetAddressOf());

#if defined(_DEBUG)
        // ==========================================================
        //  開発モード：HLSLからの動的コンパイル（魔法の杖モード）
        // ==========================================================
        _hlslPathStr  = "Shader/Material/Phong/PhongPS.hlsl";
        _hlslPathWStr = L"Shader/Material/Phong/PhongPS.hlsl";

        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

        bool compileSuccess = GpuResourceUtils::CompilePixelShader(device,
            _hlslPathWStr,
            "main",
            "ps_5_0",
            pixelShader.GetAddressOf(),
            psBlob.GetAddressOf());

        if (!compileSuccess) {
            OutputDebugStringA("\n[Warning] HLSL compilation failed. Falling back to .cso\n");
            // 失敗したら従来のコンパイル済みシェーダーを読み込むので、画面が消えません
            GpuResourceUtils::LoadPixelShader(
                device, "Assets/Shader/PhongPS.cso", pixelShader.GetAddressOf());
        }

        // 監視スタート
        _lastWriteTime = GetLastWriteTime(_hlslPathStr);
#else
        // ==========================================================
        //  製品版モード：CSOからの高速ロード（鉄壁モード）
        // ==========================================================
        GpuResourceUtils::LoadPixelShader(
            device, "Assets/Shader/PhongPS.cso", pixelShader.GetAddressOf());
#endif

	// リフレクション初期化
	InitializeReflection(device, "Assets/Shader/PhongPS.cso");
}

void PhongShader::Begin(const RenderContext& rc)
{
	// パイプラインの初期設定　バインドリセットなど
	ApplyShaderPipeline(rc.deviceContext, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void PhongShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// 1. マテリアル・プロパティの自動適用
	// "materialColor", "DiffuseMap", "NormalMap" などが全てここで処理されます
	if (mesh.material && mesh.material->data)
	{
		UpdateMaterialProperties(dc, *mesh.material->data);
	}


}

void PhongShader::End(const RenderContext& rc)
{
	// シャドウマップsrvのスロット解除
	UnbindResources(rc.deviceContext);
}

void PhongShader::CheckAndReload(ID3D11Device *device)
{
// 開発モードの時だけ監視を有効化
#if defined(_DEBUG)
    auto currentTime = GetLastWriteTime(_hlslPathStr);

    // 保存（更新）されたことを検知！
    if (currentTime > _lastWriteTime) {

        Microsoft::WRL::ComPtr<ID3D11PixelShader> newPixelShader;
        Microsoft::WRL::ComPtr<ID3DBlob>          newBlob;

        // コンパイルが通った時だけ中身を差し替える（エラーで画面が消えるのを防ぐ）
        if (GpuResourceUtils::CompilePixelShader(device,
                _hlslPathWStr,
                "main",
                "ps_5_0",
                newPixelShader.GetAddressOf(),
                newBlob.GetAddressOf())) {
            pixelShader = newPixelShader;
            OutputDebugStringA(("[HotReload] Success! Reloaded: " + _hlslPathStr + "\n").c_str());
        }

        // エラーであってもタイムスタンプは更新して、毎フレームコンパイルが走るのを防ぐ
        _lastWriteTime = currentTime;
    }
#endif
}