#include "TrailRenderer.h"
#include <algorithm>
#include "Engine/Graphics/Core/Graphics.h"

using namespace DirectX;

TrailRenderer::TrailRenderer() {

	ID3D11Device* device = Graphics::Instance().GetDevice();

    // 1. 動的頂点バッファの作成 (CPUから書き込み可能)
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(Vertex) * MAX_VERTICES;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;       // ★重要: 動的更新
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // ★重要: CPUから書込許可
    device->CreateBuffer(&vbDesc, nullptr, vertexBuffer.GetAddressOf());

    // 2. 動的インデックスバッファの作成
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(uint32_t) * MAX_INDICES;
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&ibDesc, nullptr, indexBuffer.GetAddressOf());

    // キャッシュミスを防ぐため、事前にメモリを確保しておく
    vertexData.reserve(MAX_VERTICES);
    indexData.reserve(MAX_INDICES);
}

void TrailRenderer::Render(ID3D11DeviceContext* dc, const TrailComponent& trail) {
    if (trail.count < 2) return; // 点が2つ未満なら線（面）が作れないので無視

    vertexData.clear();
    indexData.clear();

    // =================================================================
    // 1. 頂点データ（ハシゴの骨組み）の構築
    // =================================================================
    for (int i = 0; i < trail.count; ++i) {
        // リングバッファを「一番古い(Tail)」から「最新(Head)」へ辿るインデックス計算
        int index = (trail.headIndex - trail.count + 1 + i + TrailComponent::MAX_TRAIL_POINTS) % TrailComponent::MAX_TRAIL_POINTS;
        const auto& pt = trail.history[index];

        // UVの U座標 (横の長さ): 過去(0.0) → 現在(1.0)
        float u = static_cast<float>(i) / (trail.count - 1);

        // 寿命によるフェードアウト計算: 古い点ほど透明(alpha=0)になる
        float lifeRatio = std::clamp(1.0f - (pt.age / trail.lifeTime), 0.0f, 1.0f);
        XMFLOAT4 color = trail.color;
        color.w *= lifeRatio; // アルファ値に適用

        // =========================================================
        // ★追加 幅のテーパリング（鋭利化）計算
        // =========================================================
        XMVECTOR baseV = XMLoadFloat3(&pt.basePos);
        XMVECTOR tipV = XMLoadFloat3(&pt.tipPos);

        // 過去の点（lifeRatioが0に近い）ほど、先端の座標を根本に近づける
        // ※ 0.0f にすると完全に一点に収束し、0.2f くらいにすると少し太さを残して消えます
        float widthScale = std::lerp(0.0f, 1.0f, lifeRatio);
        XMVECTOR taperedTipV = XMVectorLerp(baseV, tipV, widthScale);

        XMFLOAT3 taperedTip;
        XMStoreFloat3(&taperedTip, taperedTipV);

        // 根本の頂点
        vertexData.push_back({ pt.basePos, color, XMFLOAT2(u, 0.0f) });
        // 先端の頂点をテーパリング済みの座標に差し替え
        vertexData.push_back({ taperedTip, color, XMFLOAT2(u, 1.0f) });
    }

    // =================================================================
    // 2. インデックスデータ（頂点を繋いで三角形にする順番）の構築
    // =================================================================
    // 四角形(セグメント)の数は、点の数より1つ少ない
    int segmentCount = trail.count - 1;
    for (int i = 0; i < segmentCount; ++i) {
        uint32_t baseIdx = i * 2;
        // 1つ目の三角形 (左下 -> 左上 -> 右下)
        indexData.push_back(baseIdx + 0);
        indexData.push_back(baseIdx + 1);
        indexData.push_back(baseIdx + 2);
        // 2つ目の三角形 (右下 -> 左上 -> 右上)
        indexData.push_back(baseIdx + 2);
        indexData.push_back(baseIdx + 1);
        indexData.push_back(baseIdx + 3);
    }

    // =================================================================
    // 3. GPUバッファへの流し込み (Map / Unmap)
    // =================================================================
    D3D11_MAPPED_SUBRESOURCE mappedVB;
    if (SUCCEEDED(dc->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB))) {
        memcpy(mappedVB.pData, vertexData.data(), sizeof(Vertex) * vertexData.size());
        dc->Unmap(vertexBuffer.Get(), 0);
    }

    D3D11_MAPPED_SUBRESOURCE mappedIB;
    if (SUCCEEDED(dc->Map(indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIB))) {
        memcpy(mappedIB.pData, indexData.data(), sizeof(uint32_t) * indexData.size());
        dc->Unmap(indexBuffer.Get(), 0);
    }

    // =================================================================
    // 4. 描画コマンドの発行
    // =================================================================
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    dc->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    dc->DrawIndexed(static_cast<UINT>(indexData.size()), 0, 0);
}