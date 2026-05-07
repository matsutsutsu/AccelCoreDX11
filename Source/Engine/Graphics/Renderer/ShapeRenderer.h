#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXCollision.h> 
#include <mutex>
#include <memory>
#include <atomic>
#include "Engine/Graphics/Renderer/InstanceBuffer.h"

class ShapeRenderer
{
public:
	ShapeRenderer(ID3D11Device* device);
	~ShapeRenderer() {}

	void SetCullingEnabled(bool enable) { m_cullingEnabled = enable; }

	void DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);
	void DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color);
	void DrawCapsule(const DirectX::XMFLOAT4X4& transform, float radius, float height, const DirectX::XMFLOAT4& color);
	void DrawWireframeCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);
	void DrawWireframeCircle(const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT4& rotation);
	void DrawArrow(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float headSize, const DirectX::XMFLOAT4& color);
	void DrawBone(const DirectX::XMFLOAT4X4& transform, float length, const DirectX::XMFLOAT4& color);
	void DrawLine(const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT4& color);
	void DrawTriangle(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT3& v3, const DirectX::XMFLOAT4& color);
	void DrawSolidTriangle(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT4& color);
	void DrawRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float length, const DirectX::XMFLOAT4& color);
	void DrawGrid(float spacing, int subdivisions, const DirectX::XMFLOAT4& color);
	void DrawAxis(const DirectX::XMFLOAT4X4& transform, float length);

	void Render(ID3D11DeviceContext* dc, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);

private:
	DirectX::BoundingFrustum m_frustum;
	bool                     m_cullingEnabled = true;

	bool IsVisible(const DirectX::BoundingSphere& sphere);
	bool IsVisible(const DirectX::BoundingOrientedBox& box);

	struct Mesh {
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		UINT vertexCount;
	};

	void CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh);
	void CreateBoxMesh(ID3D11Device* device, float width, float height, float depth);
	void CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions);
	void CreateHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions);
	void CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions);
	void CreateBoneMesh(ID3D11Device* device, float length);
	void CreateLineMesh(ID3D11Device* device);
	void CreateTriangleFaceMesh(ID3D11Device* device);

private:
	Mesh boxMesh;
	Mesh sphereMesh;
	Mesh halfSphereMesh;
	Mesh cylinderMesh;
	Mesh boneMesh;
	Mesh lineMesh;
	Mesh triangleFaceMesh; // 塗りつぶし三角形用の金型

	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;

	// デバッグ描画用の「常に最前面に描画する（Zテスト無効）」ステート
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_debugDepthState;

	Microsoft::WRL::ComPtr<ID3D11BlendState>        m_transparentBlendState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthTestNoWriteState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_cullNoneRasterizerState;

	// =========================================================
	// インスタンシング ＆ マルチスレッド (TLS) 対応
	// =========================================================
	InstanceBuffer _instanceBuffer;

	uint64_t _instanceId;
	static std::atomic<uint64_t> s_instanceCounter;

	struct ShapeDrawList {
		std::vector<InstanceData> boxes;
		std::vector<InstanceData> spheres;
		std::vector<InstanceData> halfSpheres;
		std::vector<InstanceData> cylinders;
		std::vector<InstanceData> bones;
		std::vector<InstanceData> lines;
		std::vector<InstanceData> solidTriangles;
	};

	std::mutex _listMutex;
	std::vector<std::unique_ptr<ShapeDrawList>> _drawLists;

	ShapeDrawList& GetDrawList();
};