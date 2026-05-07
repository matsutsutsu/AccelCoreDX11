#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
// カリング計算用ヘッダ
#include <DirectXCollision.h>
#include <mutex> // ★マルチスレッド用
#include <memory>  // ★追加
#include <atomic>  // ★追加
#include "Engine/Graphics/Renderer/InstanceBuffer.h" // ★インスタンスバッファ用
#include "Engine/Graphics/Core/PipelineState.h"  // ★PSO（金型）
#include "Engine/Graphics/Core/RenderContext.h"      // ★RenderContext

// 頂点とインデックスのバッファをまとめる構造体
struct GeometryBuffer
{
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	UINT indexCount = 0;
};

// スレッドが個別に持つ「専用の袋（バッファ）」
struct PrimitiveDrawList {
	std::vector<InstanceData> opaqueSpheres;
	std::vector<InstanceData> wireframeSpheres;
	std::vector<InstanceData> opaqueBoxes;
	std::vector<InstanceData> wireframeBoxes;
	std::vector<InstanceData> opaqueCylinders;
	std::vector<InstanceData> wireframeCylinders;
	std::vector<InstanceData> opaqueCapsules;
	std::vector<InstanceData> wireframeCapsules;
	std::vector<InstanceData> opaqueCones;
	std::vector<InstanceData> wireframeCones;
};

class PrimitiveRenderer
{
public:
	PrimitiveRenderer(ID3D11Device* device);
	~PrimitiveRenderer() {}

public:
	// 描画実行
	void Render(const RenderContext& rc, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);

	// カリングの有効/無効切り替え（デバッグで全部見たい時などに使う）
    void SetCullingEnabled(bool enable) { m_cullingEnabled = enable; }

	// ====================================================================
    // ★ 描画登録関数の引数を変更（サイズ・半径は ECS 側で行列に焼き込む）
    // ====================================================================
    void DrawSphere(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);
    void DrawWireframeSphere(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);

    void DrawBox(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);
    void DrawWireframeBox(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);

    void DrawCylinder(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);
    void DrawWireframeCylinder(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);

    void DrawCapsule(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);
    void DrawWireframeCapsule(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);

    void DrawCone(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);
    void DrawWireframeCone(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);


private:
	// ★追加: カリング判定用ヘルパー
	bool IsVisible(const DirectX::BoundingSphere& sphere);
	bool IsVisible(const DirectX::BoundingOrientedBox& box);

	// ★追加: 視錐台データ
	DirectX::BoundingFrustum m_frustum;
	bool                     m_cullingEnabled = true;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;

	// ★ バラバラだったステートを廃止し、2つの「金型」に集約
	PipelineState _psoOpaque;
	PipelineState _psoWireframe;


	GeometryBuffer	sphereBuffer;
	GeometryBuffer	halfSphereBuffer;
	GeometryBuffer	boxBuffer;
	GeometryBuffer	cylinderBuffer;
	GeometryBuffer	coneBuffer;
	GeometryBuffer  capsuleBuffer;

	InstanceBuffer _instanceBuffer; // ★ GPUへの転送バッファ

	// =========================================================
	// TLS (スレッドローカルストレージ) 管理機構
	// =========================================================
	uint64_t _instanceId; // このレンダラー固有のID
	static std::atomic<uint64_t> s_instanceCounter;
	

	std::mutex _listMutex; // 初回のポインタ登録用

	// 全スレッドの「専用の袋」の場所(ポインタ)を記憶しておく名簿
	// 生ポインタではなく、レンダラー自身がメモリの所有権を持つ（安全）
	std::vector<std::unique_ptr<PrimitiveDrawList>> _drawLists;

	// 自分のスレッドの袋を取得する関数
	PrimitiveDrawList& GetDrawList();



	// ====================================================================
	// メッシュ生成関数
	// ====================================================================
	void CreateBoxMesh(ID3D11Device* device, float radius, float height, float depth, GeometryBuffer& buffer);
	void CreateSphereMesh(ID3D11Device* device, float radius, int slices, int stacks, GeometryBuffer& buffer);
	void CreateHalfSphereMesh(ID3D11Device* device, float radius, int slices, int stacks, GeometryBuffer& buffer);
	void CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int slices, int stacks, GeometryBuffer& buffer);
	void CreateCapsuleMesh(ID3D11Device* device, float radius, float cylinderHeight, int slices, int rings);
};
