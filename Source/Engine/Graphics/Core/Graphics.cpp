#include "Graphics.h"
#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/Camera.h"

#include "Engine/Graphics/Shader/ShaderRegistry.h"

#include "Engine/Graphics/Shader/Material/BasicShader.h"
#include "Engine/Graphics/Shader/Material/LambertShader.h"
#include "Engine/Graphics/Shader/Material/OutlineShader.h"
#include "Engine/Graphics/Shader/Material/PBRShader.h"
#include "Engine/Graphics/Shader/Material/PhongShader.h"
#include "Engine/Graphics/Shader/Material/ShadowMapShader.h"
#include "Engine/Graphics/Shader/Material/ToonShader.h"

#include "Engine/Graphics/Shader/Material/TrailShader.h" // 上部に追加

// HLSLの CbScene (b8) に合わせた構造体
struct CbSceneData {
    DirectX::XMMATRIX viewProjection;
    DirectX::XMFLOAT4 lightDirection;
    DirectX::XMFLOAT4 lightColor;
    DirectX::XMFLOAT4 cameraPosition;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
};

// 初期化
void Graphics::Initialize(HWND hWnd)
{
	this->hWnd = hWnd;
	// 画面のサイズを取得する。
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT screenWidth = rc.right - rc.left;
	UINT screenHeight = rc.bottom - rc.top;

	this->screenWidth = static_cast<float>(screenWidth);
	this->screenHeight = static_cast<float>(screenHeight);

	HRESULT hr = S_OK;

	// デバイス＆スワップチェーンの生成
	{
		UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1,
		};

		// スワップチェーンを作成するための設定オプション
		DXGI_SWAP_CHAIN_DESC swapchainDesc;
		{
			swapchainDesc.BufferDesc.Width = screenWidth;
			swapchainDesc.BufferDesc.Height = screenHeight;
			swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
			swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
			swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			swapchainDesc.SampleDesc.Count = 1;
			swapchainDesc.SampleDesc.Quality = 0;
			swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchainDesc.BufferCount = 2;
			swapchainDesc.OutputWindow = hWnd;
			swapchainDesc.Windowed = TRUE;
			swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapchainDesc.Flags = 0;
		}

		D3D_FEATURE_LEVEL featureLevel;

		// デバイス＆スワップチェーンの生成
		hr = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			createDeviceFlags,
			featureLevels,
			ARRAYSIZE(featureLevels),
			D3D11_SDK_VERSION,
			&swapchainDesc,
			swapchain.GetAddressOf(),
			device.GetAddressOf(),
			&featureLevel,
			immediateContext.GetAddressOf()
		);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	// レンダーターゲットビューの生成
	{
		// スワップチェーンからバックバッファテクスチャを取得する。
		// ※スワップチェーンに内包されているバックバッファテクスチャは'色'を書き込むテクスチャ。
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = swapchain->GetBuffer(
			0,
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(texture2d.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		// バックバッファテクスチャへの書き込みの窓口となるレンダーターゲットビューを生成する。
		hr = device->CreateRenderTargetView(texture2d.Get(), nullptr, renderTargetView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	// 深度ステンシルビューの生成
	{
		// 深度ステンシル情報を書き込むためのテクスチャを作成する。
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		D3D11_TEXTURE2D_DESC texture2dDesc;
		texture2dDesc.Width = screenWidth;
		texture2dDesc.Height = screenHeight;
		texture2dDesc.MipLevels = 1;
		texture2dDesc.ArraySize = 1;
		texture2dDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		texture2dDesc.SampleDesc.Count = 1;
		texture2dDesc.SampleDesc.Quality = 0;
		texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
		texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texture2dDesc.CPUAccessFlags = 0;
		texture2dDesc.MiscFlags = 0;
		hr = device->CreateTexture2D(&texture2dDesc, nullptr, texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		// 深度ステンシルテクスチャへの書き込みに窓口になる深度ステンシルビューを作成する。
		hr = device->CreateDepthStencilView(texture2d.Get(), nullptr, depthStencilView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	// ビューポート
	{
		viewport.Width = static_cast<float>(screenWidth);
		viewport.Height = static_cast<float>(screenHeight);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
	}

	// レンダーステート生成
	renderState = std::make_unique<RenderState>(device.Get());

	// レンダラ生成
	primitiveRenderer = std::make_unique<PrimitiveRenderer>(device.Get());
	shapeRenderer = std::make_unique<ShapeRenderer>(device.Get());
	modelRenderer = std::make_unique<ModelRenderer>(device.Get());
	shadowMap = std::make_unique<ShadowMap>(device.Get(), 2048, 2048);
	particleRenderer = std::make_unique<ParticleRenderer>();
    particleRenderer->Initialize(device.Get());
	trailRenderer = std::make_unique<TrailRenderer>();

	// enum ではなく、文字列のハッシュ値を使ってシェーダーファクトリを登録します
	ShaderRegistry::Instance().RegisterShader<BasicShader>("Basic"_hash);
	ShaderRegistry::Instance().RegisterShader<LambertShader>("Lambert"_hash);
	ShaderRegistry::Instance().RegisterShader<PhongShader>("Phong"_hash);
	ShaderRegistry::Instance().RegisterShader<ToonShader>("Toon"_hash);
	ShaderRegistry::Instance().RegisterShader<OutlineShader>("Outline"_hash);
	ShaderRegistry::Instance().RegisterShader<ShadowMapShader>("ShadowMap"_hash);
	ShaderRegistry::Instance().RegisterShader<PBRShader>("PBR"_hash);

	// トレイルシェーダーの登録
	ShaderRegistry::Instance().RegisterShader<TrailShader>("Trail"_hash);

	// ゲームビュー用RTT作成
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = (UINT)screenWidth;
	texDesc.Height = (UINT)screenHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	device->CreateTexture2D(&texDesc, nullptr, gameTex.GetAddressOf());
	device->CreateRenderTargetView(gameTex.Get(), nullptr, gameRTV.GetAddressOf());
	device->CreateShaderResourceView(gameTex.Get(), nullptr, gameSRV.GetAddressOf());

	// シーン定数バッファ作成 (b8用)
    if (!_sceneConstantBuffer) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth         = sizeof(CbSceneData);
        // 16バイトアライメント調整
        if (desc.ByteWidth % 16 != 0) desc.ByteWidth = ((desc.ByteWidth + 15) / 16) * 16;

        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;

		// device変数はメンバ変数を使用
        HRESULT hr = device->CreateBuffer(&desc, nullptr, _sceneConstantBuffer.GetAddressOf());
        if (FAILED(hr)) {
            // エラーハンドリング (OutputDebugStringなど)
        }
    }
}

// 毎フレーム呼び出す更新処理
void Graphics::Update(Camera* camera)
{
    // カメラ情報の更新
    if (_sceneConstantBuffer && camera) {
        CbSceneData data;

        // --- 行列のコピー ---
        // 引数の camera から取得
        DirectX::XMFLOAT4X4 vp = camera->GetViewProjectionMatrix();
        data.viewProjection    = DirectX::XMLoadFloat4x4(&vp);

        data.view       = DirectX::XMLoadFloat4x4(&camera->GetView());
        data.projection = DirectX::XMLoadFloat4x4(&camera->GetProjection());

        // --- カメラ位置 ---
        const auto &pos     = camera->GetEye();
        data.cameraPosition = DirectX::XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);

        // --- ライト情報 (仮) ---
        data.lightDirection = DirectX::XMFLOAT4(0.5f, -1.0f, 0.5f, 0.0f);
        data.lightColor     = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        // GPUへ転送
        immediateContext->UpdateSubresource(_sceneConstantBuffer.Get(), 0, nullptr, &data, 0, 0);
    }
}

// クリア
void Graphics::Clear(float r, float g, float b, float a,bool useGameRTV)
{
	float color[4]{ r, g, b, a };

	if (useGameRTV)
		immediateContext->ClearRenderTargetView(gameRTV.Get(), color);
	else
		immediateContext->ClearRenderTargetView(renderTargetView.Get(), color);

	immediateContext->ClearDepthStencilView(depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

// レンダーターゲット設定
void Graphics::SetRenderTargets(bool useGameRTV)
{
	if (useGameRTV)
		immediateContext->OMSetRenderTargets(1, gameRTV.GetAddressOf(), depthStencilView.Get());
	else
		immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());

	immediateContext->RSSetViewports(1, &viewport);
}

// 画面表示
void Graphics::Present(UINT syncInterval)
{
	swapchain->Present(syncInterval, 0);
}


void Graphics::Shutdown()
{
    // 1. デバイスコンテキストのステートをクリア (Refcount: 0 の原因を消す)
    if (immediateContext) {
        immediateContext->ClearState();
        immediateContext->Flush();
    }

    // 2. ライブオブジェクトのレポート
    if (device) {
        Microsoft::WRL::ComPtr<ID3D11Debug> debug;
        // デバッグインターフェースを取得
        HRESULT hr = device.As(&debug);
        if (SUCCEEDED(hr)) {
            // 詳細を出力しつつ、内部オブジェクト(Ignore Internal)は無視する
            //debug->ReportLiveObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
        }
    }

    // 3. メンバ変数のComPtrをリセット (明示的に解放)
    // これをやらないと、Graphicsのデストラクタが走るまでDeviceが生き残り、
    // Reportの時点で「Deviceが生きてる」と言われてしまいます。

    // 生成と逆順に消していくのが行儀が良いです
    _sceneConstantBuffer.Reset();
    gameSRV.Reset();
    gameRTV.Reset();
    gameTex.Reset();

    shadowMap.reset();
    modelRenderer.reset();
    shapeRenderer.reset();
    primitiveRenderer.reset();
	particleRenderer.reset();
	trailRenderer.reset();
    renderState.reset();

    depthStencilView.Reset();
    renderTargetView.Reset();
    swapchain.Reset();
    immediateContext.Reset();

    // 最後にデバイスを離す
    device.Reset();
}