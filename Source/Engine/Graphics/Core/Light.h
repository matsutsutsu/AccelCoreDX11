#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include <wrl/client.h>

//並行光源
struct DirectionalLight
{
	DirectX::XMFLOAT3	direction = { 0.7f, -0.3f, 0.4f };
	float intensity = 0.3f;
	DirectX::XMFLOAT3	color = { 1, 1, 1 };
	float padding = 0.0f; // 16バイト境界への合わせ

	// --- 半球ライティング用 ---
	DirectX::XMFLOAT4 skyColor = { 0.3f, 0.4f, 0.6f, 1.0f };    // wは強度
	DirectX::XMFLOAT4 groundColor = { 0.1f, 0.1f, 0.05f, 1.0f }; // wは強度
};

//点光源
struct PointLights
{
	DirectX::XMFLOAT3 position = { 0, 0, 0 };
	float range = 10.0f;
	DirectX::XMFLOAT3 color = { 1, 1, 1 };
	float intensity = 1.0f;
	// 減衰率と有効フラグ
	float constantAttenuation = 1.0f;
	float linearAttenuation = 0.0f;
	float quadraticAttenuation = 1.0f;
	float active = 1.0f; // 1.0=ON, 0.0=OFF
};

//スポットライト
struct SpotLights
{
	DirectX::XMFLOAT3 position = { 0, 0, 0 };
	float range = 10.0f;

	DirectX::XMFLOAT3 direction = { 0, -1, 0 };
	float innerCos = 0.99f;

	DirectX::XMFLOAT3 color = { 1, 1, 1 };
	float outerCos = 0.9f;

	float intensity = 1.0f;
	float active = 1.0f;
	float padding1 = 0.0f;
	float padding2 = 0.0f;

	// 減衰率
	float constantAttenuation = 1.0f;
	float linearAttenuation = 0.0f;
	float quadraticAttenuation = 1.0f;
	float padding = 0.0f;
};



class LightManager
{
public:
	static constexpr int MAX_POINT_LIGHTS = 8;
	static constexpr int MAX_SPOT_LIGHTS = 8;

	// GPUに送る定数バッファの最終形態
	struct LightConstantBuffer {
		DirectionalLight dirLight;
		PointLights pointLights[MAX_POINT_LIGHTS];
		SpotLights spotLights[MAX_SPOT_LIGHTS];
		int pointLightCount = 0;
		int spotLightCount = 0;
		float padding[2] = { 0.0f,0.0f };
	};

	LightManager() = default;

	// 初期化：定数バッファの作成
	void Initialize(ID3D11Device* device);

	// CPU上のデータをGPUの定数バッファに転送し、パイプラインにバインドする
	void Bind(ID3D11DeviceContext* dc, int slot);
	// GUI描画（Sceneから詳細を追い出す）
	void DrawGUI();
	// 全クリア
	void Clear() { pointLights.clear(); spotLights.clear(); }

	// ディレクショナルライト設定
	void SetDirectionalLight(DirectionalLight& light) { cbData.dirLight = light; }
	// ディレクショナルライト取得
	const DirectionalLight& GetDirectionalLight() const { return cbData.dirLight; }
	DirectionalLight& GetDirectionalLight() { return cbData.dirLight; }

	// 点光源追加
	void AddPointLight(const PointLights& light) {
		if (pointLights.size() < MAX_POINT_LIGHTS) pointLights.push_back(light);
	}

	// スポットライト追加
	void AddSpotLight(const SpotLights& light) {
		if (spotLights.size() < MAX_SPOT_LIGHTS) spotLights.push_back(light);
	}

	// どの名前の定数バッファとしてHLSLに定義されているべきか（規格化）
	static const char* GetBufferName() { return "CbLight"; }

	// 取得用
	const DirectX::XMFLOAT4X4& GetLightViewProjection() const { return m_lightViewProjection; }

private:
	LightConstantBuffer cbData;

	std::vector<PointLights> pointLights;
	std::vector<SpotLights> spotLights;

	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

	// シャドウマップ
	DirectX::XMFLOAT4X4 m_lightViewProjection = {	// ライト視点の行列
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	}; 


};
