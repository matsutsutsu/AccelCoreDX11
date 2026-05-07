#include "PrimitiveRenderer.h"
#include <stdio.h>
#include <math.h> // std::isnan用
#include <memory>
#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Core/Light.h"
#include "Engine/Graphics/Shader/ShaderResources.h"


#include "tracy/Tracy.hpp"


using namespace DirectX;

namespace {
	// メッシュ作成時に使用する頂点構造体
	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
	};
}



// 静的メンバの実体化
std::atomic<uint64_t> PrimitiveRenderer::s_instanceCounter = 0;

PrimitiveRenderer::PrimitiveRenderer(ID3D11Device* device)
{

    // インスタンスごとの固有IDを発行する（シーン再読み込み時のバグ防止）
    _instanceId = ++s_instanceCounter;

	// ==============================================================
    // 1. シェーダーと入力レイアウトの作成 (既存コードそのまま)
    // ==============================================================

	// 入力レイアウト
	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

    // ★ GpuResourceUtils を使って、最新の .cso ファイルをロードする
        // ※パス ("Assets/Shader/...") は、あなたのプロジェクトの .cso 出力先に合わせてください
    GpuResourceUtils::LoadVertexShader(
        device,
        "Assets/Shader/PrimitiveRendererVS.cso",
        inputElementDesc,
        _countof(inputElementDesc),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf()
    );

    GpuResourceUtils::LoadPixelShader(
        device,
        "Assets/Shader/PrimitiveRendererPS.cso",
        pixelShader.GetAddressOf()
    );
	// ==============================================================
	// 2. PSO（金型）の作成 と インスタンスバッファの初期化
	// ==============================================================
	// ① 不透明（塗りつぶし）用
	_psoOpaque = PipelineState(PipelineStateDesc::DefaultOpaque());

	// ② ワイヤーフレーム用（ベースをもらって、Rasterizerだけ書き換える）
	PipelineStateDesc wireDesc = PipelineStateDesc::DefaultOpaque();
	wireDesc.rasterizer = RasterizerState::WireCullNone; // 線画＆裏面も描画
	_psoWireframe = PipelineState(wireDesc);

	// インスタンスバッファ生成
	_instanceBuffer.Create(device);


	// ==============================================================
	// 3. 基本メッシュの生成 (サイズ1の基準図形)
	// ==============================================================
	CreateSphereMesh(device, 1.0f, 16, 16, sphereBuffer);
	CreateHalfSphereMesh(device, 1.0f, 16, 16, halfSphereBuffer);
	CreateCylinderMesh(device, 1.0f, 1.0f, -0.5f, 1.0f, 16, 1, cylinderBuffer);
	CreateCylinderMesh(device, 1.0f, 0.0f, 0.0f, 1.0f, 16, 1, coneBuffer);
	CreateBoxMesh(device, 1.0f, 1.0f, 1.0f, boxBuffer);
    CreateCapsuleMesh(device, 1.0f, 1.0f, 32, 16);
}

// =========================================================
// カリング判定用ヘルパー実装
// =========================================================
bool PrimitiveRenderer::IsVisible(const DirectX::BoundingSphere &sphere)
{
    if (!m_cullingEnabled) return true;
    return m_frustum.Intersects(sphere);
}

bool PrimitiveRenderer::IsVisible(const DirectX::BoundingOrientedBox &box)
{
    if (!m_cullingEnabled) return true;
    return m_frustum.Intersects(box);
}



// 1. バッファの取得と登録（ロックは初回1回のみ）
PrimitiveDrawList& PrimitiveRenderer::GetDrawList() {
//
//    ZoneScopedN("PrimitiveRenderer::GetDrawList"); // シーン描画全体
//#endif

    // thread_local をつけると、スレッドごとに実体が別々に作られる
	// staticだと全スレッドで共有されてしまうけど、thread_localならスレッドごとに自分専用のバッファを持てる！
    // スレッド側には「データの実体」ではなく「ポインタ」と「所属ID」だけをキャッシュさせる
    thread_local PrimitiveDrawList* myLocalList = nullptr;
    thread_local uint64_t myRendererId = 0;

    // もし「今のレンダラー」に未登録なら、新しい袋を作ってもらう
    if (myRendererId != _instanceId) {
        std::lock_guard<std::mutex> lock(_listMutex);

        // メモリの確保(袋の作成)はレンダラー側で行い、所有権を渡す
        auto newList = std::make_unique<PrimitiveDrawList>();

        // =========================================================
        // メモリの事前予約（お引越し・mallocを完全に防ぐ！）
        // これを書くだけで、push_back が一切ロックなしの爆速になります。
        // =========================================================
        //newList->opaqueBoxes.reserve(20000);
        //newList->wireframeBoxes.reserve(5000);
        //newList->opaqueSpheres.reserve(20000);
        // ※よく使う図形はあらかじめ巨大な枠を確保しておく

        myLocalList = newList.get();
        _drawLists.push_back(std::move(newList)); // レンダラーが寿命を管理する

        myRendererId = _instanceId; // 登録完了
    }
    // ロックなしで爆速で返す
    return *myLocalList;
}




// ====================================================================
// 一括描画処理（インスタンシング ＆ PSO）
// ====================================================================
void PrimitiveRenderer::Render(const RenderContext& rc, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
    ZoneScopedN("PrimitiveRenderer::Render"); // シーン描画全体


	ID3D11DeviceContext* dc = rc.deviceContext;

	// 1. 次のフレームのカリング用に視錐台を更新
	BoundingFrustum frustumLocal;
	BoundingFrustum::CreateFromMatrix(frustumLocal, XMLoadFloat4x4(&projection));
	XMMATRIX V = XMLoadFloat4x4(&view);
	XMMATRIX invV = XMMatrixInverse(nullptr, V);
	frustumLocal.Transform(m_frustum, invV);

	// 2. 共通シェーダーの設定
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	dc->IASetInputLayout(inputLayout.Get());

	// ★ Graphicsから全シーン共通の定数バッファ(CbScene)をもらってセット
	ID3D11Buffer* sceneCB = Graphics::Instance().GetSceneConstantBuffer();
	if (sceneCB) {
		dc->VSSetConstantBuffers(SLOT_CB_SCENE, 1, &sceneCB);
		dc->PSSetConstantBuffers(SLOT_CB_SCENE, 1, &sceneCB);
	}

    // ====================================================================
    //  ライトマネージャーに「最新のライト情報(CbLight)を送ってくれ」と頼む
    // ====================================================================
    if (rc.lightManager) {
        // SLOT_CB_LIGHT (通常は9番) に、ライトマネージャー自身がバインドしてくれる！
        rc.lightManager->Bind(dc, SLOT_CB_LIGHT); 
    }
    

	// 3. インスタンシング描画用のラムダ式ヘルパー
    auto DrawInstanced = [&](GeometryBuffer& geo, const PipelineState& pso, std::vector<InstanceData> PrimitiveDrawList::* memberPtr) 
        {

		// ★ PSO（金型）の適用：これでBlendもDepthもRasterizerも完璧に設定される
		pso.Apply(dc, rc.renderState);

		// 頂点・インデックスのセット (POSITION:12byte + NORMAL:12byte = 24byte)
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		dc->IASetVertexBuffers(0, 1, geo.vertexBuffer.GetAddressOf(), &stride, &offset);
		dc->IASetIndexBuffer(geo.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        // =========================================================
        // ★最重要修正：Render中に他スレッドが名簿を破壊するのを防ぐ！
        // =========================================================
        std::lock_guard<std::mutex> lock(_listMutex);

        // 名簿に載っている全スレッドのバッファを巡回して描画
        for (auto& listPtr : _drawLists) {
            // unique_ptr から生ポインタを取り出し、メンバポインタ演算子を適用する
            // 同時に、参照(instances)の宣言と初期化を一行で行う
            std::vector<InstanceData>& instances = (*listPtr).*memberPtr;

            if (instances.empty()) continue;

            size_t totalCount = instances.size();

            // ★防弾：もし totalCount が異常な数（数千万など）になっていたらメモリが壊れているのでスキップ
            if (totalCount > 100000) {
                instances.clear();
                continue;
            }

            size_t drawOffset = 0;

            while (drawOffset < totalCount) {
                // InstanceBufferの限界数(4096)を超えないようにチャンク分け
                size_t drawCount = (std::min)(totalCount - drawOffset, InstanceBuffer::MAX_INSTANCES);

                // 一時的なスライス作成
                std::vector<InstanceData> chunk(instances.begin() + drawOffset, instances.begin() + drawOffset + drawCount);

                // GPUへ書き込みとバインド
                _instanceBuffer.Write(dc, chunk);
                _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

                dc->DrawIndexedInstanced(geo.indexCount, (UINT)drawCount, 0, 0, 0);

                drawOffset += drawCount;
            }
            // 描画が完了したバッファをリセット
            instances.clear();
        }
		};

	// 4. 全ての図形を一気に描画
    DrawInstanced(boxBuffer, _psoOpaque, &PrimitiveDrawList::opaqueBoxes);
    DrawInstanced(boxBuffer, _psoWireframe, &PrimitiveDrawList::wireframeBoxes);

    DrawInstanced(sphereBuffer, _psoOpaque, &PrimitiveDrawList::opaqueSpheres);
    DrawInstanced(sphereBuffer, _psoWireframe, &PrimitiveDrawList::wireframeSpheres);

    DrawInstanced(cylinderBuffer, _psoOpaque, &PrimitiveDrawList::opaqueCylinders);
    DrawInstanced(cylinderBuffer, _psoWireframe, &PrimitiveDrawList::wireframeCylinders);

    DrawInstanced(coneBuffer, _psoOpaque, &PrimitiveDrawList::opaqueCones);
    DrawInstanced(coneBuffer, _psoWireframe, &PrimitiveDrawList::wireframeCones);

    DrawInstanced(capsuleBuffer, _psoOpaque, &PrimitiveDrawList::opaqueCapsules);
    DrawInstanced(capsuleBuffer, _psoWireframe, &PrimitiveDrawList::wireframeCapsules);

	// 5. 描画後のクリーンアップ（SRVの解除）
    _instanceBuffer.Unbind(dc, SLOT_SRV_INSTANCE);

    // =========================================================
    // ステート・リークの防止（GPUの設定をデフォルトに戻す）
    // =========================================================
    dc->RSSetState(nullptr); // ラスタライザ（カリング等）のリセット
    dc->OMSetDepthStencilState(nullptr, 0); // 深度テストのリセット
    dc->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // ブレンドのリセット

    dc->VSSetShader(nullptr, nullptr, 0); // シェーダーの解除
    dc->PSSetShader(nullptr, nullptr, 0);
}


// ====================================================================
// 描画登録処理（マルチスレッド安全）
// ====================================================================
void PrimitiveRenderer::DrawBox(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    // もし ECS 側からゴミデータが流れてきたら、描画せずに無視する（宇宙サイズの箱を防ぐ！）
    //if (!IsValidMatrix(transform)) return;

    GetDrawList().opaqueBoxes.push_back({ transform, color });
}
void PrimitiveRenderer::DrawWireframeBox(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().wireframeBoxes.push_back({ transform, color });
}
void PrimitiveRenderer::DrawSphere(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().opaqueSpheres.push_back({ transform, color });
}
void PrimitiveRenderer::DrawWireframeSphere(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().wireframeSpheres.push_back({ transform, color });
}
void PrimitiveRenderer::DrawCylinder(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().opaqueCylinders.push_back({ transform, color });
}
void PrimitiveRenderer::DrawWireframeCylinder(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().wireframeCylinders.push_back({ transform, color });
}
void PrimitiveRenderer::DrawCapsule(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().opaqueCapsules.push_back({ transform, color });
}
void PrimitiveRenderer::DrawWireframeCapsule(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().wireframeCapsules.push_back({ transform, color });
}
void PrimitiveRenderer::DrawCone(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().opaqueCones.push_back({ transform, color });
}
void PrimitiveRenderer::DrawWireframeCone(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) {
    GetDrawList().wireframeCones.push_back({ transform, color });
}



// ====================================================================
// メッシュ生成処理 (既存コードの移植)
// ====================================================================

// 球メッシュ作成
void PrimitiveRenderer::CreateSphereMesh(ID3D11Device* device, float radius, int slices, int stacks, GeometryBuffer& buffer)
{
    int vertex_count = slices * (stacks - 1) + 2;
    int index_count = (slices - 1) * 3
        + 3
        + (stacks - 2) * (slices - 1) * 6
        + (stacks - 2) * 6
        + (slices - 1) * 3
        + 3;
    std::unique_ptr<Vertex[]> vertices = std::make_unique<Vertex[]>(vertex_count);
    std::unique_ptr<UINT[]> indices = std::make_unique<UINT[]>(index_count);

    std::unique_ptr<float[]> cos_phi = std::make_unique<float[]>(stacks);
    std::unique_ptr<float[]> sin_phi = std::make_unique<float[]>(stacks);
    float phi_step = DirectX::XM_PI / stacks;

    std::unique_ptr<float[]> cos_theta = std::make_unique<float[]>(slices);
    std::unique_ptr<float[]> sin_theta = std::make_unique<float[]>(slices);
    float theta_step = DirectX::XM_2PI / slices;

    float phi = 0;
    for (int s = 0; s < stacks; s++, phi += phi_step)
    {
        sin_phi[s] = sinf(phi);
        cos_phi[s] = cosf(phi);
    }

    float theta = 0;
    for (int s = 0; s < slices; s++, theta += theta_step)
    {
        sin_theta[s] = sinf(theta);
        cos_theta[s] = cosf(theta);
    }

    Vertex* vertex = vertices.get();

    // north pole. 
    vertex->position.x = 0;
    vertex->position.y = radius;
    vertex->position.z = 0;

    DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&vertex->position);
    DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(Position);
    DirectX::XMStoreFloat3(&vertex->normal, Normal);
    vertex++;

    for (int s = 1; s < stacks; s++)
    {
        float y = radius * cos_phi[s];
        float r = radius * sin_phi[s];
        for (int l = 0; l < slices; l++)
        {
            vertex->position.x = r * sin_theta[l];
            vertex->position.y = y;
            vertex->position.z = r * cos_theta[l];

            Position = DirectX::XMLoadFloat3(&vertex->position);
            Normal = DirectX::XMVector3Normalize(Position);
            DirectX::XMStoreFloat3(&vertex->normal, Normal);
            vertex++;
        }
    }

    // south_pole
    vertex->position.x = 0;
    vertex->position.y = -radius;
    vertex->position.z = 0;

    Position = DirectX::XMLoadFloat3(&vertex->position);
    Normal = DirectX::XMVector3Normalize(Position);
    DirectX::XMStoreFloat3(&vertex->normal, Normal);

    UINT* index = indices.get();

    // north pole cap
    for (int l = 1; l < slices; l++)
    {
        index[0] = l + 1; index[1] = 0; index[2] = l;
        index += 3;
    }
    index[0] = 1; index[1] = 0; index[2] = slices;
    index += 3;

    for (int s = 0; s < (stacks - 2); s++)
    {
        int l = 1;
        for (; l < slices; l++)
        {
            index[0] = ((s + 1) * slices + l + 1);
            index[1] = (s * slices + l + 1);
            index[2] = (s * slices + l);
            index[3] = ((s + 1) * slices + l);
            index[4] = ((s + 1) * slices + l + 1);
            index[5] = (s * slices + l);
            index += 6;
        }
        index[0] = ((s + 1) * slices + 1);
        index[1] = (s * slices + 1);
        index[2] = (s * slices + slices);
        index[3] = ((s + 1) * slices + slices);
        index[4] = ((s + 1) * slices + 1);
        index[5] = (s * slices + slices);
        index += 6;
    }

    // south pole cap
    int base_index = slices * (stacks - 2);
    int last_index = vertex_count - 1;
    for (int l = 1; l < slices; l++)
    {
        index[0] = (base_index + l); index[1] = (last_index); index[2] = (base_index + l + 1);
        index += 3;
    }
    index[0] = (base_index + slices); index[1] = last_index; index[2] = base_index + 1;

    // バッファ作成
    D3D11_BUFFER_DESC desc = {};
    D3D11_SUBRESOURCE_DATA subData = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertex_count);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    subData.pSysMem = vertices.get();
    device->CreateBuffer(&desc, &subData, buffer.vertexBuffer.GetAddressOf());

    desc.ByteWidth = static_cast<UINT>(sizeof(UINT) * index_count);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    subData.pSysMem = indices.get();
    device->CreateBuffer(&desc, &subData, buffer.indexBuffer.GetAddressOf());

    buffer.indexCount = index_count;
}

// 半球メッシュ作成
void PrimitiveRenderer::CreateHalfSphereMesh(ID3D11Device* device, float radius, int slices, int stacks, GeometryBuffer& buffer)
{
    int vertex_count = slices * (stacks - 1) + 1;
    int index_count = (slices - 1) * 3 + 3 + (stacks - 2) * (slices - 1) * 6 + (stacks - 2) * 6;

    std::unique_ptr<Vertex[]> vertices = std::make_unique<Vertex[]>(vertex_count);
    std::unique_ptr<UINT[]> indices = std::make_unique<UINT[]>(index_count);

    std::unique_ptr<float[]> cos_phi = std::make_unique<float[]>(stacks);
    std::unique_ptr<float[]> sin_phi = std::make_unique<float[]>(stacks);
    float phi_step = DirectX::XM_PI / (stacks - 1) * 0.5f;

    std::unique_ptr<float[]> cos_theta = std::make_unique<float[]>(slices);
    std::unique_ptr<float[]> sin_theta = std::make_unique<float[]>(slices);
    float theta_step = DirectX::XM_2PI / slices;

    float phi = 0;
    for (int s = 0; s < stacks; s++, phi += phi_step)
    {
        sin_phi[s] = sinf(phi); cos_phi[s] = cosf(phi);
    }
    float theta = 0;
    for (int s = 0; s < slices; s++, theta += theta_step)
    {
        sin_theta[s] = sinf(theta); cos_theta[s] = cosf(theta);
    }

    Vertex* vertex = vertices.get();
    vertex->position.x = 0; vertex->position.y = radius; vertex->position.z = 0;
    DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&vertex->position);
    DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(Position);
    DirectX::XMStoreFloat3(&vertex->normal, Normal);
    vertex++;

    for (int s = 1; s < stacks; s++)
    {
        float y = radius * cos_phi[s];
        float r = radius * sin_phi[s];
        for (int l = 0; l < slices; l++)
        {
            vertex->position.x = r * sin_theta[l];
            vertex->position.y = y;
            vertex->position.z = r * cos_theta[l];

            Position = DirectX::XMLoadFloat3(&vertex->position);
            Normal = DirectX::XMVector3Normalize(Position);
            DirectX::XMStoreFloat3(&vertex->normal, Normal);
            vertex++;
        }
    }

    UINT* index = indices.get();
    for (int l = 1; l < slices; l++)
    {
        index[0] = l + 1; index[1] = 0; index[2] = l;
        index += 3;
    }
    index[0] = 1; index[1] = 0; index[2] = slices;
    index += 3;

    for (int s = 0; s < (stacks - 2); s++)
    {
        int l = 1;
        for (; l < slices; l++)
        {
            index[0] = ((s + 1) * slices + l + 1); index[1] = (s * slices + l + 1); index[2] = (s * slices + l);
            index[3] = ((s + 1) * slices + l);     index[4] = ((s + 1) * slices + l + 1); index[5] = (s * slices + l);
            index += 6;
        }
        index[0] = ((s + 1) * slices + 1);      index[1] = (s * slices + 1);      index[2] = (s * slices + slices);
        index[3] = ((s + 1) * slices + slices); index[4] = ((s + 1) * slices + 1); index[5] = (s * slices + slices);
        index += 6;
    }

    D3D11_BUFFER_DESC desc = {};
    D3D11_SUBRESOURCE_DATA subData = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertex_count);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    subData.pSysMem = vertices.get();
    device->CreateBuffer(&desc, &subData, buffer.vertexBuffer.GetAddressOf());

    desc.ByteWidth = static_cast<UINT>(sizeof(UINT) * index_count);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    subData.pSysMem = indices.get();
    device->CreateBuffer(&desc, &subData, buffer.indexBuffer.GetAddressOf());

    buffer.indexCount = index_count;
}

// 箱メッシュ作成
void PrimitiveRenderer::CreateBoxMesh(ID3D11Device* device, float radius, float height, float depth, GeometryBuffer& buffer)
{
    Vertex vertices[] = {
        // 正面
        { DirectX::XMFLOAT3(-radius, +height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
        { DirectX::XMFLOAT3(+radius, +height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
        { DirectX::XMFLOAT3(-radius, -height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
        { DirectX::XMFLOAT3(+radius, -height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
        // 背面
        { DirectX::XMFLOAT3(-radius, +height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
        { DirectX::XMFLOAT3(+radius, +height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
        { DirectX::XMFLOAT3(-radius, -height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
        { DirectX::XMFLOAT3(+radius, -height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
        // 右面
        { DirectX::XMFLOAT3(+radius, +height, -depth), DirectX::XMFLOAT3(1, 0, 0) },
        { DirectX::XMFLOAT3(+radius, +height, +depth), DirectX::XMFLOAT3(1, 0, 0) },
        { DirectX::XMFLOAT3(+radius, -height, -depth), DirectX::XMFLOAT3(1, 0, 0) },
        { DirectX::XMFLOAT3(+radius, -height, +depth), DirectX::XMFLOAT3(1, 0, 0) },
        // 左面
        { DirectX::XMFLOAT3(-radius, +height, -depth), DirectX::XMFLOAT3(-1, 0, 0) },
        { DirectX::XMFLOAT3(-radius, +height, +depth), DirectX::XMFLOAT3(-1, 0, 0) },
        { DirectX::XMFLOAT3(-radius, -height, -depth), DirectX::XMFLOAT3(-1, 0, 0) },
        { DirectX::XMFLOAT3(-radius, -height, +depth), DirectX::XMFLOAT3(-1, 0, 0) },
        // 上面
        { DirectX::XMFLOAT3(-radius, +height, +depth), DirectX::XMFLOAT3(0, 1, 0) },
        { DirectX::XMFLOAT3(+radius, +height, +depth), DirectX::XMFLOAT3(0, 1, 0) },
        { DirectX::XMFLOAT3(-radius, +height, -depth), DirectX::XMFLOAT3(0, 1, 0) },
        { DirectX::XMFLOAT3(+radius, +height, -depth), DirectX::XMFLOAT3(0, 1, 0) },
        // 下面
        { DirectX::XMFLOAT3(-radius, -height, +depth), DirectX::XMFLOAT3(0, -1, 0) },
        { DirectX::XMFLOAT3(+radius, -height, +depth), DirectX::XMFLOAT3(0, -1, 0) },
        { DirectX::XMFLOAT3(-radius, -height, -depth), DirectX::XMFLOAT3(0, -1, 0) },
        { DirectX::XMFLOAT3(+radius, -height, -depth), DirectX::XMFLOAT3(0, -1, 0) },
    };

    UINT indices[] = {
        0, 1, 2,  2, 1, 3,
        5, 4, 7,  7, 4, 6,
        8, 9, 10, 10, 9, 11,
        13, 12, 15, 15, 12, 14,
        16, 17, 18, 18, 17, 19,
        21, 20, 23, 23, 20, 22,
    };

    D3D11_BUFFER_DESC desc = {};
    D3D11_SUBRESOURCE_DATA subData = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(vertices));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    subData.pSysMem = vertices;
    device->CreateBuffer(&desc, &subData, buffer.vertexBuffer.GetAddressOf());

    desc.ByteWidth = static_cast<UINT>(sizeof(indices));
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    subData.pSysMem = indices;
    device->CreateBuffer(&desc, &subData, buffer.indexBuffer.GetAddressOf());

    buffer.indexCount = sizeof(indices) / sizeof(indices[0]);
}

// 円柱メッシュ作成
void PrimitiveRenderer::CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int slices, int stacks, GeometryBuffer& buffer)
{
    float stack_height = height / stacks;
    float radius_step = (radius2 - radius1) / stacks;
    int num_rings = stacks + 1;

    int vertex_count = num_rings * (slices + 1);
    int index_count = stacks * slices * 6;
    if (radius1 > 0) { vertex_count += slices + 2; index_count += slices * 3; }
    if (radius2 > 0) { vertex_count += slices + 2; index_count += slices * 6; }

    std::unique_ptr<Vertex[]> vertices = std::make_unique<Vertex[]>(vertex_count);
    std::unique_ptr<UINT[]> indices = std::make_unique<UINT[]>(index_count);

    Vertex* vertex = vertices.get();
    int vcount = 0;

    for (int i = 0; i < num_rings; ++i)
    {
        float y = start + i * stack_height;
        float r = radius1 + i * radius_step;
        float y_next = start + (i + 1) * stack_height;
        float r_next = radius1 + (i + 1) * radius_step;

        float d_theta = DirectX::XM_2PI / slices;
        for (int j = 0; j <= slices; ++j)
        {
            float c = cosf(j * d_theta);
            float s = sinf(j * d_theta);

            DirectX::XMFLOAT3 t = { -s, 0.0f, c };
            DirectX::XMFLOAT3 p = { r * c, y, r * s };
            DirectX::XMFLOAT3 p_next = { r_next * c, y_next, r_next * s };

            DirectX::XMFLOAT3 n;
            DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&p);
            DirectX::XMVECTOR P_Next = DirectX::XMLoadFloat3(&p_next);
            DirectX::XMVECTOR B = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(P, P_Next));
            DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&t);
            DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
            DirectX::XMStoreFloat3(&n, N);

            p.z *= -1; n.z *= -1;
            vertex->position = p;
            vertex->normal = n;

            vertex++; vcount++;
        }
    }

    int num_ring_vertices = slices + 1;
    UINT* index = indices.get();

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            index[0] = (i * num_ring_vertices + j);
            index[1] = ((i + 1) * num_ring_vertices + j + 1);
            index[2] = ((i + 1) * num_ring_vertices + j);

            index[3] = (i * num_ring_vertices + j);
            index[4] = (i * num_ring_vertices + j + 1);
            index[5] = ((i + 1) * num_ring_vertices + j + 1);
            index += 6;
        }
    }

    if (radius1 > 0)
    {
        int base_index = vcount;
        float y = start;
        float d_theta = DirectX::XM_2PI / slices;
        for (int i = 0; i <= slices; ++i)
        {
            vertex->position.x = radius1 * cosf(i * d_theta);
            vertex->position.y = y;
            vertex->position.z = -radius1 * sinf(i * d_theta);
            vertex->normal = { 0.0f, -1.0f, 0.0f };
            vertex++; vcount++;
        }
        vertex->position = { 0.0f, y, 0.0f };
        vertex->normal = { 0.0f, -1.0f, 0.0f };
        vertex++; vcount++;

        int center_index = vcount - 1;
        for (int i = 0; i < slices; ++i)
        {
            index[0] = center_index; index[1] = base_index + i + 1; index[2] = base_index + i;
            index += 3;
        }
    }

    if (radius2 > 0)
    {
        int base_index = vcount;
        float y = start + height;
        float d_theta = DirectX::XM_2PI / slices;
        for (int i = 0; i <= slices; ++i)
        {
            vertex->position.x = radius2 * cosf(i * d_theta);
            vertex->position.y = y;
            vertex->position.z = -radius2 * sinf(i * d_theta);
            vertex->normal = { 0.0f, 1.0f, 0.0f };
            vertex++; vcount++;
        }
        vertex->position = { 0.0f, y, 0.0f };
        vertex->normal = { 0.0f, 1.0f, 0.0f };
        vertex++; vcount++;

        int center_index = vcount - 1;
        for (int i = 0; i < slices; ++i)
        {
            index[0] = center_index; index[1] = base_index + i + 1; index[2] = base_index + i;
            index[3] = center_index; index[4] = base_index + i; index[5] = base_index + i + 1;
            index += 6;
        }
    }

    D3D11_BUFFER_DESC desc = {};
    D3D11_SUBRESOURCE_DATA subData = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertex_count);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    subData.pSysMem = vertices.get();
    device->CreateBuffer(&desc, &subData, buffer.vertexBuffer.GetAddressOf());

    desc.ByteWidth = static_cast<UINT>(sizeof(UINT) * index_count);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    subData.pSysMem = indices.get();
    device->CreateBuffer(&desc, &subData, buffer.indexBuffer.GetAddressOf());

    buffer.indexCount = index_count;
}

// ====================================================================
// カプセルメッシュの生成（球の赤道分割アルゴリズム）
// ====================================================================
void PrimitiveRenderer::CreateCapsuleMesh(ID3D11Device* device, float radius, float cylinderHeight, int slices, int rings)
{
    // 球を綺麗に半分に割るため、リング数は必ず偶数にする
    if (rings % 2 != 0) rings++;

    float halfHeight = cylinderHeight * 0.5f;
    int numRings = rings + 2; // 円柱部分を作るため、赤道リングを2つに分裂させる（+2）

    int vertexCount = numRings * (slices + 1);
    int indexCount = (numRings - 1) * slices * 6;

    std::vector<Vertex> vertices(vertexCount);
    std::vector<uint32_t> indices(indexCount);

    // ----------------------------------------
    // 1. 頂点の計算
    // ----------------------------------------
    int vIndex = 0;
    for (int i = 0; i < numRings; ++i)
    {
        float phi;
        float yOffset;

        // 上半球（北半球）の計算
        if (i <= rings / 2) {
            phi = (DirectX::XM_PI * i) / rings; // 0 から π/2 まで
            yOffset = halfHeight;               // 上に持ち上げる
        }
        // 下半球（南半球）の計算
        else {
            phi = (DirectX::XM_PI * (i - 1)) / rings; // π/2 から π まで
            yOffset = -halfHeight;                    // 下に押し下げる
        }

        float y = radius * cosf(phi);
        float r = radius * sinf(phi);

        for (int j = 0; j <= slices; ++j)
        {
            float theta = (DirectX::XM_2PI * j) / slices;
            float x = r * cosf(theta);
            float z = r * sinf(theta);

            vertices[vIndex].position = { x, y + yOffset, z };

            // 法線は「オフセット移動する前の球体の向き」をそのまま使う
            float len = sqrtf(x * x + y * y + z * z);
            if (len > 0.0001f) {
                vertices[vIndex].normal = { x / len, y / len, z / len };
            }
            else {
                vertices[vIndex].normal = { 0.0f, 1.0f, 0.0f }; // 頂点の極における安全対策
            }
            vIndex++;
        }
    }

    // ----------------------------------------
    // 2. インデックス（面）の計算
    // ----------------------------------------
    int iIndex = 0;
    for (int i = 0; i < numRings - 1; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int nextI = i + 1;
            int nextJ = j + 1;

            // 1つ目の三角形
            indices[iIndex++] = i * (slices + 1) + j;
            indices[iIndex++] = nextI * (slices + 1) + j;
            indices[iIndex++] = i * (slices + 1) + nextJ;

            // 2つ目の三角形
            indices[iIndex++] = i * (slices + 1) + nextJ;
            indices[iIndex++] = nextI * (slices + 1) + j;
            indices[iIndex++] = nextI * (slices + 1) + nextJ;
        }
    }

    // ----------------------------------------
    // 3. GPUバッファの作成
    // ----------------------------------------
    D3D11_BUFFER_DESC vDesc = {};
    vDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertexCount);
    vDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vData = { vertices.data(), 0, 0 };
    device->CreateBuffer(&vDesc, &vData, &capsuleBuffer.vertexBuffer);

    D3D11_BUFFER_DESC iDesc = {};
    iDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indexCount);
    iDesc.Usage = D3D11_USAGE_IMMUTABLE;
    iDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iData = { indices.data(), 0, 0 };
    device->CreateBuffer(&iDesc, &iData, &capsuleBuffer.indexBuffer);

    capsuleBuffer.indexCount = indexCount;
}