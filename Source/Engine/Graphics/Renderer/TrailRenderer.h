#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <DirectXMath.h>
#include "Game/Logics/Combat/Shader/TrailComponent.h"

class TrailRenderer {
public:
    // トレイル専用の頂点フォーマット
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 uv;
    };

    TrailRenderer();
    ~TrailRenderer() = default;

    // ECSシステムから呼ばれ、1つのトレイルを描画する
    void Render(ID3D11DeviceContext* dc, const TrailComponent& trail);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

    // CPU側で一時的に頂点を組み立てるための配列（毎フレームのnew/deleteを防ぐためメンバに持つ）
    std::vector<Vertex> vertexData;
    std::vector<uint32_t> indexData;

    static constexpr int MAX_VERTICES = TrailComponent::MAX_TRAIL_POINTS * 2; // 根本と先端で2倍
    static constexpr int MAX_INDICES = (TrailComponent::MAX_TRAIL_POINTS - 1) * 6; // 四角形1つにつき6インデックス
};