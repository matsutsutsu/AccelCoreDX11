#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "RenderState.h"
#include <string>
#include "Engine/Graphics/Renderer/PrimitiveRenderer.h"
#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Renderer/ParticleRenderer.h"
#include "Engine/Graphics/Renderer/TrailRenderer.h"
#include "Engine/Graphics/Shader/Pass/ShadowMap.h"

class Camera;


// D3Dオブジェクトに名前を付けるヘルパー関数
template <typename T> void SetDebugName(T *child, const std::string &name)
{
    if (child) {
        child->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.c_str());
    }
}

// グラフィックス
class Graphics
{
private:
	Graphics() = default;
	~Graphics() = default;

public:
	// インスタンス取得
	static Graphics& Instance()
	{
		static Graphics instance;
		return instance;
	}

	// 初期化
	void Initialize(HWND hWnd);

	// 毎フレーム呼び出してシーン情報を更新する
    void Update(Camera* camera);

	// クリア
	void Clear(float r, float g, float b, float a,bool useGameRTV);

	// レンダーターゲット設定
	void SetRenderTargets(bool useGameRTV);

	// 画面表示
	void Present(UINT syncInterval);

	// ウインドウハンドル取得
	HWND GetWindowHandle() { return hWnd; }

	// デバイス取得
	ID3D11Device* GetDevice() { return device.Get(); }

	// デバイスコンテキスト取得
	ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }

	// スクリーン幅取得
	float GetScreenWidth() const { return screenWidth; }

	// スクリーン高さ取得
	float GetScreenHeight() const { return screenHeight; }

	// レンダーステート取得
	RenderState* GetRenderState() { return renderState.get(); }

	// プリミティブレンダラ取得
	PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }

	// シェイプレンダラ取得
	ShapeRenderer* GetShapeRenderer() const { return shapeRenderer.get(); }

	// モデルレンダラ取得
	ModelRenderer* GetModelRenderer() const { return modelRenderer.get(); }

	// パーティクルレンダラ取得
	ParticleRenderer *GetParticleRenderer() const { return particleRenderer.get(); }

	// トレイルレンダラ取得
	TrailRenderer* GetTrailRenderer() const { return trailRenderer.get(); }

	// シャドウマップ取得
	ShadowMap* GetShadowMap() const { return shadowMap.get(); }

	// 深度ステンシルビューの取得 (これで解決！)
	ID3D11DepthStencilView* GetDepthStencilView() { return depthStencilView.Get(); }

	// バックバッファ(画面)のRTV取得 (ポストプロセスの最終出力先に使う)
	ID3D11RenderTargetView* GetBackBufferRTV() { return renderTargetView.Get(); }

	// シーン定数バッファ (b8: CbScene) を取得する関数
    ID3D11Buffer *GetSceneConstantBuffer() const { return _sceneConstantBuffer.Get(); }

	ID3D11RenderTargetView* GetGameRTV() { return gameRTV.Get(); }
	ID3D11ShaderResourceView* GetGameSRV() { return gameSRV.Get(); }


        void Shutdown(); // これを追加

private:
	HWND											hWnd = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Device>			device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		immediateContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>			swapchain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	depthStencilView;
	D3D11_VIEWPORT									viewport = {};

	float	screenWidth = 0;
	float	screenHeight = 0;

	std::unique_ptr<RenderState>					renderState;
	std::unique_ptr<PrimitiveRenderer>				primitiveRenderer;
	std::unique_ptr<ShapeRenderer>					shapeRenderer;
	std::unique_ptr<ModelRenderer>					modelRenderer;
    std::unique_ptr<ParticleRenderer>               particleRenderer;
	std::unique_ptr<TrailRenderer>                  trailRenderer;	

	std::unique_ptr<ShadowMap> shadowMap;

	// シーン情報用定数バッファ (b8)
	Microsoft::WRL::ComPtr<ID3D11Buffer> _sceneConstantBuffer;

	// --- ゲームビュー用追加 ---
	Microsoft::WRL::ComPtr<ID3D11Texture2D> gameTex;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gameRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gameSRV;


};
