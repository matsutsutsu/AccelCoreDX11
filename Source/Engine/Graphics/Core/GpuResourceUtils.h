#pragma once

#include <d3d11.h>
#include <string>
#include <d3dcompiler.h> // コンパイラ用ヘッダー

// GPUリソースユーティリティ
class GpuResourceUtils
{
public:
	// 頂点シェーダー読み込み
	static HRESULT LoadVertexShader(
		ID3D11Device* device,
		const char* filename,
		const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
		UINT inputElementCount,
		ID3D11InputLayout** inputLayout,
		ID3D11VertexShader** vertexShader);

	// ピクセルシェーダー読み込み
	static HRESULT LoadPixelShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11PixelShader** pixelShader);

	// テクスチャ読み込み
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const char* filename,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// テクスチャ読み込み
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const void* data,
		size_t size,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// ダミーテクスチャ作成
	static HRESULT CreateDummyTexture(
		ID3D11Device* device,
		UINT color,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// 定数バッファ作成
	static HRESULT CreateConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

	// DYNAMIC用の定数バッファ作成
	static HRESULT CreateDynamicConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

	// 頂点バッファ作成
    static HRESULT CreateVertexBuffer(
        ID3D11Device *device, const void *data, UINT size, ID3D11Buffer **buffer);

	// インデックスバッファ作成
    static HRESULT CreateIndexBuffer(ID3D11Device *device,
        const UINT                                *data, // uint32_t を UINT に変更
        UINT                                       size,
        ID3D11Buffer                             **buffer);


	// HLSLから直接ピクセルシェーダーをコンパイルする関数
    static bool CompilePixelShader(ID3D11Device *device,
        const std::wstring                      &hlslPath,
        const char                              *entryPoint,
        const char                              *shaderModel,
        ID3D11PixelShader                      **pixelShader,
        ID3DBlob                               **outShaderBlob = nullptr);

};
