#include "ShapeRenderer.h"
#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <algorithm>

#include "tracy/Tracy.hpp"


using namespace DirectX;

std::atomic<uint64_t> ShapeRenderer::s_instanceCounter = 0;

// ヘルパー
bool ShapeRenderer::IsVisible(const DirectX::BoundingSphere& sphere) {
    if (!m_cullingEnabled) return true;
    return m_frustum.Intersects(sphere);
}
bool ShapeRenderer::IsVisible(const DirectX::BoundingOrientedBox& box) {
    if (!m_cullingEnabled) return true;
    return m_frustum.Intersects(box);
}

// ====================================================================
// コンストラクタ
// ====================================================================
ShapeRenderer::ShapeRenderer(ID3D11Device* device)
{
    _instanceId = ++s_instanceCounter;

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/ShapeRendererVS.cso", inputElementDesc, _countof(inputElementDesc), inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
    GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/ShapeRendererPS.cso", pixelShader.GetAddressOf());

    _instanceBuffer.Create(device); // インスタンスバッファ初期化

    CreateBoxMesh(device, 1.0f, 1.0f, 1.0f);
    CreateSphereMesh(device, 1.0f, 32);
    CreateHalfSphereMesh(device, 1.0f, 32);
    CreateCylinderMesh(device, 1.0f, 1.0f, -0.5f, 1.0f, 32);
    CreateBoneMesh(device, 1.0f);
    CreateLineMesh(device);
    CreateTriangleFaceMesh(device);

    // 1. 半透明ブレンドステート
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, m_transparentBlendState.GetAddressOf());

    // 2. 深度ステート（Zテストはするが、Z書き込みは絶対にしない）※点滅防止の要
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // ★書き込みOFF
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsDesc, m_depthTestNoWriteState.GetAddressOf());

    // 3. ラスタライザステート（カリングOFF＝両面描画する）
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE; // ★裏面も描く
    // =========================================================
    // ★ここが点滅を完全に殺す魔法のパラメータ
    // =========================================================
    // 深度バッファの精度に対して、少しだけカメラ側に引き寄せる（マイナス値）
    rsDesc.DepthBias = -1000;            // 固定値のオフセット
    rsDesc.DepthBiasClamp = 0.0f;
    // 斜めから見下ろした時の誤差を打ち消す（非常に重要）
    rsDesc.SlopeScaledDepthBias = -2.0f;
    // =========================================================

    device->CreateRasterizerState(&rsDesc, m_cullNoneRasterizerState.GetAddressOf());


    // Zテストを無効化（常に上書き描画する）ステートの作成
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = FALSE; // ★最重要: 深度テストをしない
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 深度バッファに書き込まない
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;     // 常にテスト通過

    HRESULT hr = device->CreateDepthStencilState(&depthDesc, m_debugDepthState.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), L"Failed to create debug depth stencil state");
}

// ====================================================================
// マルチスレッド用 TLS バッファ取得
// ====================================================================
ShapeRenderer::ShapeDrawList& ShapeRenderer::GetDrawList() {
    thread_local ShapeDrawList* myLocalList = nullptr;
    thread_local uint64_t myRendererId = 0;

    if (myRendererId != _instanceId) {
        std::lock_guard<std::mutex> lock(_listMutex);
        auto newList = std::make_unique<ShapeDrawList>();

        // メモリの事前予約（大渋滞を防ぐ）
        // Debug用の線は大量に出るため、Lineは特に多く確保しておく
        newList->boxes.reserve(5000);
        newList->spheres.reserve(5000);
        newList->halfSpheres.reserve(5000);
        newList->cylinders.reserve(5000);
        newList->bones.reserve(1000);
        newList->lines.reserve(50000);

        myLocalList = newList.get();
        _drawLists.push_back(std::move(newList));
        myRendererId = _instanceId;
    }
    return *myLocalList;
}

// ====================================================================
// 描画登録
// ====================================================================
void ShapeRenderer::DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color)
{
    if (m_cullingEnabled) {
        BoundingOrientedBox obb;
        obb.Center = position;
        obb.Extents = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
        XMVECTOR Q = XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);
        XMStoreFloat4(&obb.Orientation, Q);
        if (!m_frustum.Intersects(obb)) return;
    }

    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(size.x, size.y, size.z);
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

    InstanceData data;
    XMStoreFloat4x4(&data.world, S * R * T);
    data.customParams = color;
    GetDrawList().boxes.push_back(data);
}

void ShapeRenderer::DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color)
{
    if (m_cullingEnabled) {
        BoundingSphere sphere;
        sphere.Center = position;
        sphere.Radius = radius;
        if (!IsVisible(sphere)) return;
    }

    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

    InstanceData data;
    XMStoreFloat4x4(&data.world, S * T);
    data.customParams = color;
    GetDrawList().spheres.push_back(data);
}

void ShapeRenderer::DrawCapsule(const DirectX::XMFLOAT4X4& transform, float radius, float height, const DirectX::XMFLOAT4& color)
{
    if (m_cullingEnabled) {
        XMMATRIX M = XMLoadFloat4x4(&transform);
        XMVECTOR scale, rot, trans;
        XMMatrixDecompose(&scale, &rot, &trans, M);
        float maxScale = XMVectorGetX(XMVectorMax(scale, XMVectorMax(XMVectorSwizzle<1, 2, 3, 0>(scale), scale)));
        float boundingRadius = (height * 0.5f) + radius;
        BoundingSphere sphere;
        XMStoreFloat3(&sphere.Center, trans);
        sphere.Radius = boundingRadius * maxScale;
        if (!IsVisible(sphere)) return;
    }

    float cylinderHeight = (std::max)(0.0f, height - 2.0f * radius);
    float halfCylinderHeight = cylinderHeight * 0.5f;

    XMMATRIX Transform = XMLoadFloat4x4(&transform);
    XMVECTOR AxisX = XMVectorScale(Transform.r[0], radius);
    XMVECTOR AxisY = XMVectorScale(Transform.r[1], radius);
    XMVECTOR AxisZ = XMVectorScale(Transform.r[2], radius);

    XMVECTOR TopOffset = XMVectorSet(0, halfCylinderHeight, 0, 0);
    XMVECTOR BottomOffset = XMVectorSet(0, -halfCylinderHeight, 0, 0);

    ShapeDrawList& list = GetDrawList();

    // 上半球
    {
        XMVECTOR Position = XMVectorAdd(Transform.r[3], XMVector3TransformNormal(TopOffset, Transform));
        XMMATRIX World;
        World.r[0] = AxisX; World.r[1] = AxisY; World.r[2] = AxisZ; World.r[3] = XMVectorSetW(Position, 1.0f);

        InstanceData data;
        XMStoreFloat4x4(&data.world, World);
        data.customParams = color;
        list.halfSpheres.push_back(data);
    }
    // 円柱
    if (cylinderHeight > 0.0001f) {
        XMMATRIX World;
        World.r[0] = AxisX; World.r[1] = XMVectorScale(Transform.r[1], cylinderHeight); World.r[2] = AxisZ; World.r[3] = Transform.r[3];

        InstanceData data;
        XMStoreFloat4x4(&data.world, World);
        data.customParams = color;
        list.cylinders.push_back(data);
    }
    // 下半球
    {
        XMVECTOR Position = XMVectorAdd(Transform.r[3], XMVector3TransformNormal(BottomOffset, Transform));
        XMMATRIX World;
        World.r[0] = AxisX; World.r[1] = XMVectorNegate(AxisY); World.r[2] = XMVectorNegate(AxisZ); World.r[3] = XMVectorSetW(Position, 1.0f);

        InstanceData data;
        XMStoreFloat4x4(&data.world, World);
        data.customParams = color;
        list.halfSpheres.push_back(data);
    }
}

void ShapeRenderer::DrawWireframeCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color)
{
    if (m_cullingEnabled) {
        float r = sqrtf(radius * radius + (height * 0.5f) * (height * 0.5f));
        BoundingSphere sphere;
        sphere.Center = position;
        sphere.Radius = r;
        if (!IsVisible(sphere)) return;
    }

    float halfHeight = height * 0.5f;
    XMFLOAT3 topPos = { position.x, position.y + halfHeight, position.z };
    XMFLOAT3 btmPos = { position.x, position.y - halfHeight, position.z };

    XMFLOAT4 xRotation;
    XMStoreFloat4(&xRotation, XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0, 0));

    DrawWireframeCircle(topPos, radius, color, xRotation);
    DrawWireframeCircle(btmPos, radius, color, xRotation);
    DrawLine({ position.x - radius, topPos.y, position.z }, { position.x - radius, btmPos.y, position.z }, color);
    DrawLine({ position.x + radius, topPos.y, position.z }, { position.x + radius, btmPos.y, position.z }, color);
    DrawLine({ position.x, topPos.y, position.z - radius }, { position.x, btmPos.y, position.z - radius }, color);
    DrawLine({ position.x, topPos.y, position.z + radius }, { position.x, btmPos.y, position.z + radius }, color);
}

void ShapeRenderer::DrawWireframeCircle(const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT4& rotation)
{
    const int segments = 24;
    const float step = XM_2PI / segments;
    XMVECTOR Q = XMLoadFloat4(&rotation);
    XMVECTOR C = XMLoadFloat3(&center);

    XMVECTOR P_prev_local = XMVectorSet(radius, 0, 0, 0);
    P_prev_local = XMVector3Rotate(P_prev_local, Q);
    XMVECTOR P_prev = XMVectorAdd(C, P_prev_local);

    for (int i = 1; i <= segments; ++i) {
        float theta = i * step;
        XMVECTOR P_curr_local = XMVectorSet(radius * cosf(theta), radius * sinf(theta), 0, 0);
        P_curr_local = XMVector3Rotate(P_curr_local, Q);
        XMVECTOR P_curr = XMVectorAdd(C, P_curr_local);

        XMFLOAT3 p1, p2;
        XMStoreFloat3(&p1, P_prev);
        XMStoreFloat3(&p2, P_curr);
        DrawLine(p1, p2, color);
        P_prev = P_curr;
    }
}

void ShapeRenderer::DrawArrow(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float headSize, const DirectX::XMFLOAT4& color)
{
    DrawLine(start, end, color);
    XMVECTOR VStart = XMLoadFloat3(&start);
    XMVECTOR VEnd = XMLoadFloat3(&end);
    XMVECTOR Dir = XMVectorSubtract(VEnd, VStart);
    float length = XMVectorGetX(XMVector3Length(Dir));
    if (length < 0.001f) return;
    Dir = XMVector3Normalize(Dir);

    XMVECTOR Base = XMVectorSubtract(VEnd, XMVectorScale(Dir, headSize));
    XMVECTOR Up = XMVectorSet(0, 1, 0, 0);
    if (fabsf(XMVectorGetY(Dir)) > 0.99f) Up = XMVectorSet(0, 0, 1, 0);

    XMVECTOR Right = XMVector3Normalize(XMVector3Cross(Up, Dir));
    Up = XMVector3Normalize(XMVector3Cross(Dir, Right));

    float halfHead = headSize * 0.5f;
    XMVECTOR P1 = XMVectorAdd(Base, XMVectorScale(Right, halfHead));
    XMVECTOR P2 = XMVectorSubtract(Base, XMVectorScale(Right, halfHead));
    XMVECTOR P3 = XMVectorAdd(Base, XMVectorScale(Up, halfHead));
    XMVECTOR P4 = XMVectorSubtract(Base, XMVectorScale(Up, halfHead));

    XMFLOAT3 tip, p1, p2, p3, p4;
    XMStoreFloat3(&tip, VEnd);
    XMStoreFloat3(&p1, P1);
    XMStoreFloat3(&p2, P2);
    XMStoreFloat3(&p3, P3);
    XMStoreFloat3(&p4, P4);

    DrawLine(tip, p1, color); DrawLine(tip, p2, color); DrawLine(tip, p3, color); DrawLine(tip, p4, color);
    DrawLine(p1, p3, color); DrawLine(p3, p2, color); DrawLine(p2, p4, color); DrawLine(p4, p1, color);
}

void ShapeRenderer::DrawBone(const DirectX::XMFLOAT4X4& transform, float length, const DirectX::XMFLOAT4& color)
{
    DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&transform);
    W.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(W.r[0]), length);
    W.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(W.r[1]), length);
    W.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(W.r[2]), length);

    InstanceData data;
    DirectX::XMStoreFloat4x4(&data.world, W);
    data.customParams = color;
    GetDrawList().bones.push_back(data);
}

void ShapeRenderer::DrawLine(const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT4& color)
{
    if (m_cullingEnabled) {
        XMVECTOR V1 = XMLoadFloat3(&p1);
        XMVECTOR V2 = XMLoadFloat3(&p2);
        XMVECTOR Diff = V2 - V1;
        float radius = XMVectorGetX(XMVector3Length(Diff)) * 0.5f;
        BoundingSphere sphere;
        XMStoreFloat3(&sphere.Center, (V1 + V2) * 0.5f);
        sphere.Radius = radius;
        if (!m_frustum.Intersects(sphere)) return;
    }

    XMVECTOR V1 = XMLoadFloat3(&p1);
    XMVECTOR V2 = XMLoadFloat3(&p2);
    XMVECTOR Diff = XMVectorSubtract(V2, V1);
    XMVECTOR LengthVec = XMVector3Length(Diff);
    float length;
    XMStoreFloat(&length, LengthVec);

    if (length < 0.0001f) return;

    XMVECTOR Forward = XMVectorScale(Diff, 1.0f / length);
    XMVECTOR Up = XMVectorSet(0, 1, 0, 0);
    if (fabsf(XMVectorGetY(Forward)) > 0.99f) Up = XMVectorSet(0, 0, 1, 0);

    XMVECTOR Right = XMVector3Normalize(XMVector3Cross(Up, Forward));
    Up = XMVector3Normalize(XMVector3Cross(Forward, Right));

    XMMATRIX World;
    World.r[0] = Right;
    World.r[1] = Up;
    World.r[2] = XMVectorScale(Forward, length);
    World.r[3] = XMVectorSetW(V1, 1.0f);

    InstanceData data;
    XMStoreFloat4x4(&data.world, World);
    data.customParams = color;
    GetDrawList().lines.push_back(data);
}

void ShapeRenderer::DrawTriangle(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT3& v3, const DirectX::XMFLOAT4& color) {
    DrawLine(v1, v2, color); DrawLine(v2, v3, color); DrawLine(v3, v1, color);
}

// ★最強のトリック：任意の3点を Transform 行列に変換して描画キューに積む
void ShapeRenderer::DrawSolidTriangle(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT4& color)
{
    using namespace DirectX;
    XMVECTOR p0 = XMLoadFloat3(&v0);
    XMVECTOR p1 = XMLoadFloat3(&v1);
    XMVECTOR p2 = XMLoadFloat3(&v2);

    // v0 を原点としたときの、2つの辺のベクトル（X軸とZ軸に割り当てる）
    XMVECTOR X = XMVectorSubtract(p1, p0);
    XMVECTOR Z = XMVectorSubtract(p2, p0);
    // 外積でY軸（法線）を作る
    XMVECTOR Y = XMVector3Cross(Z, X);

    // アフィン変換行列の構築
    XMMATRIX M;
    // ====================================================================
    // ★大修正：XMVectorSelectの罠を避け、安全にW成分を設定する
    // ====================================================================
    M.r[0] = XMVectorSetW(X, 0.0f);  // X軸のベクトル (w=0)
    M.r[1] = XMVectorSetW(Y, 0.0f);  // Y軸のベクトル (w=0)
    M.r[2] = XMVectorSetW(Z, 0.0f);  // Z軸のベクトル (w=0)
    M.r[3] = XMVectorSetW(p0, 1.0f); // 平行移動成分   (w=1)

    InstanceData data;
    XMStoreFloat4x4(&data.world, M);
    data.customParams = color;

    // TLSリスト（または mutex制御下のリスト）に積む
    GetDrawList().solidTriangles.push_back(data);
}

void ShapeRenderer::DrawRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float length, const DirectX::XMFLOAT4& color) {
    XMVECTOR O = XMLoadFloat3(&origin);
    XMVECTOR D = XMLoadFloat3(&direction);
    XMVECTOR End = XMVectorAdd(O, XMVectorScale(XMVector3Normalize(D), length));
    XMFLOAT3 endPos; XMStoreFloat3(&endPos, End);
    DrawLine(origin, endPos, color);
}

void ShapeRenderer::DrawGrid(float spacing, int subdivisions, const DirectX::XMFLOAT4& color) {
    float halfSize = spacing * subdivisions * 0.5f;
    for (int i = 0; i <= subdivisions; ++i) {
        float z = -halfSize + i * spacing;
        DrawLine({ -halfSize, 0, z }, { halfSize, 0, z }, color);
    }
    for (int i = 0; i <= subdivisions; ++i) {
        float x = -halfSize + i * spacing;
        DrawLine({ x, 0, -halfSize }, { x, 0,  halfSize }, color);
    }
}

void ShapeRenderer::DrawAxis(const DirectX::XMFLOAT4X4& transform, float length) {
    XMMATRIX M = XMLoadFloat4x4(&transform);
    XMVECTOR Pos = M.r[3];
    XMVECTOR XEnd = XMVectorAdd(Pos, XMVectorScale(XMVector3Normalize(M.r[0]), length));
    XMVECTOR YEnd = XMVectorAdd(Pos, XMVectorScale(XMVector3Normalize(M.r[1]), length));
    XMVECTOR ZEnd = XMVectorAdd(Pos, XMVectorScale(XMVector3Normalize(M.r[2]), length));
    XMFLOAT3 p, x, y, z;
    XMStoreFloat3(&p, Pos); XMStoreFloat3(&x, XEnd); XMStoreFloat3(&y, YEnd); XMStoreFloat3(&z, ZEnd);
    DrawLine(p, x, { 1.0f, 0.0f, 0.0f, 1.0f });
    DrawLine(p, y, { 0.0f, 1.0f, 0.0f, 1.0f });
    DrawLine(p, z, { 0.0f, 0.0f, 1.0f, 1.0f });
}

// ====================================================================
// メッシュ生成
// ====================================================================
void ShapeRenderer::CreateLineMesh(ID3D11Device* device) {
    std::vector<DirectX::XMFLOAT3> vertices;
    vertices.emplace_back(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
    vertices.emplace_back(DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
    CreateMesh(device, vertices, lineMesh);
}

void ShapeRenderer::CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT3) * vertices.size());
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA subData = {};
    subData.pSysMem = vertices.data();
    device->CreateBuffer(&desc, &subData, mesh.vertexBuffer.GetAddressOf());
    mesh.vertexCount = static_cast<UINT>(vertices.size());
}

void ShapeRenderer::CreateBoxMesh(ID3D11Device* device, float width, float height, float depth) {
    DirectX::XMFLOAT3 pos[8] = {
        { -width,  height, -depth}, {  width,  height, -depth}, {  width,  height,  depth}, { -width,  height,  depth},
        { -width, -height, -depth}, {  width, -height, -depth}, {  width, -height,  depth}, { -width, -height,  depth}
    };
    std::vector<DirectX::XMFLOAT3> vertices = {
        pos[0], pos[1], pos[1], pos[2], pos[2], pos[3], pos[3], pos[0],
        pos[4], pos[5], pos[5], pos[6], pos[6], pos[7], pos[7], pos[4],
        pos[0], pos[4], pos[1], pos[5], pos[2], pos[6], pos[3], pos[7]
    };
    CreateMesh(device, vertices, boxMesh);
}

void ShapeRenderer::CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions) {
    float step = DirectX::XM_2PI / subdivisions;
    std::vector<DirectX::XMFLOAT3> vertices;
    for (int i = 0; i < subdivisions; ++i) for (int j = 0; j < 2; ++j) {
        float theta = step * ((i + j) % subdivisions);
        vertices.push_back({ sinf(theta) * radius, 0.0f, cosf(theta) * radius });
    }
    for (int i = 0; i < subdivisions; ++i) for (int j = 0; j < 2; ++j) {
        float theta = step * ((i + j) % subdivisions);
        vertices.push_back({ sinf(theta) * radius, cosf(theta) * radius, 0.0f });
    }
    for (int i = 0; i < subdivisions; ++i) for (int j = 0; j < 2; ++j) {
        float theta = step * ((i + j) % subdivisions);
        vertices.push_back({ 0.0f, sinf(theta) * radius, cosf(theta) * radius });
    }
    CreateMesh(device, vertices, sphereMesh);
}

void ShapeRenderer::CreateHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions) {
    std::vector<DirectX::XMFLOAT3> vertices;
    float theta_step = DirectX::XM_2PI / subdivisions;
    for (int i = 0; i < subdivisions; ++i) for (int j = 0; j < 2; ++j) {
        float theta = theta_step * ((i + j) % subdivisions);
        vertices.push_back({ sinf(theta) * radius, 0.0f, cosf(theta) * radius });
    }
    for (int i = 0; i < subdivisions / 2; ++i) for (int j = 0; j < 2; ++j) {
        float theta = theta_step * ((i + j) % subdivisions) - DirectX::XM_PIDIV2;
        vertices.push_back({ sinf(theta) * radius, cosf(theta) * radius, 0.0f });
    }
    for (int i = 0; i < subdivisions / 2; ++i) for (int j = 0; j < 2; ++j) {
        float theta = theta_step * ((i + j) % subdivisions);
        vertices.push_back({ 0.0f, sinf(theta) * radius, cosf(theta) * radius });
    }
    CreateMesh(device, vertices, halfSphereMesh);
}

void ShapeRenderer::CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions) {
    std::vector<DirectX::XMFLOAT3> vertices;
    float theta_step = DirectX::XM_2PI / subdivisions;
    for (int i = 0; i < subdivisions; ++i) for (int j = 0; j < 2; ++j) {
        float theta = theta_step * ((i + j) % subdivisions);
        vertices.push_back({ sinf(theta) * radius1, start, cosf(theta) * radius1 });
    }
    for (int i = 0; i < subdivisions; ++i) for (int j = 0; j < 2; ++j) {
        float theta = theta_step * ((i + j) % subdivisions);
        vertices.push_back({ sinf(theta) * radius2, start + height, cosf(theta) * radius2 });
    }
    vertices.push_back({ 0.0f, start, radius1 }); vertices.push_back({ 0.0f, start + height, radius2 });
    vertices.push_back({ 0.0f, start, -radius1 }); vertices.push_back({ 0.0f, start + height, -radius2 });
    vertices.push_back({ radius1, start, 0.0f }); vertices.push_back({ radius2, start + height, 0.0f });
    vertices.push_back({ -radius1, start, 0.0f }); vertices.push_back({ -radius2, start + height, 0.0f });
    CreateMesh(device, vertices, cylinderMesh);
}

void ShapeRenderer::CreateBoneMesh(ID3D11Device* device, float length) {
    float w = length * 0.25f;
    DirectX::XMFLOAT3 pos[6] = { {0,0,0}, {w,0,w}, {0,0,length}, {-w,0,w}, {0,w,w}, {0,-w,w} };
    std::vector<DirectX::XMFLOAT3> vertices = {
        pos[0], pos[1], pos[1], pos[2], pos[2], pos[3], pos[3], pos[0],
        pos[0], pos[4], pos[4], pos[2], pos[2], pos[5], pos[5], pos[0],
        pos[1], pos[4], pos[4], pos[3], pos[3], pos[5], pos[5], pos[1]
    };
    CreateMesh(device, vertices, boneMesh);
}

// ==========================================
// ShapeRenderer.cpp の金型作成関数
// ==========================================
void ShapeRenderer::CreateTriangleFaceMesh(ID3D11Device* device)
{
    // ★大修正：余計な Normal などを一切省き、他の図形と全く同じ XMFLOAT3 (12バイト) だけにする！
    std::vector<DirectX::XMFLOAT3> vertices = {
        DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
        DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f)
    };

    // ★あなたのエンジンに既に用意されている、安全なバッファ生成関数を使う
    CreateMesh(device, vertices, triangleFaceMesh);
}

// ====================================================================
// 描画実行
// ====================================================================
void ShapeRenderer::Render(ID3D11DeviceContext* dc, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
    ZoneScopedN("ShapeRenderer::Render");


    BoundingFrustum frustumLocal;
    BoundingFrustum::CreateFromMatrix(frustumLocal, XMLoadFloat4x4(&projection));
    XMMATRIX V = XMLoadFloat4x4(&view);
    XMMATRIX invV = XMMatrixInverse(nullptr, V);
    frustumLocal.Transform(m_frustum, invV);


    // 描画前の現在の深度ステートを保存しておく
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDepthState;
    UINT oldStencilRef = 0;
    dc->OMGetDepthStencilState(oldDepthState.GetAddressOf(), &oldStencilRef);

    // デバッグ用の「透視（Zテスト無効）」ステートをセット！
    //dc->OMSetDepthStencilState(m_debugDepthState.Get(), 0);


    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->IASetInputLayout(inputLayout.Get());

    // CbMeshではなく、エンジンの共通バッファ(CbScene)を使う
    ID3D11Buffer* sceneCB = Graphics::Instance().GetSceneConstantBuffer();
    if (sceneCB) {
        dc->VSSetConstantBuffers(SLOT_CB_SCENE, 1, &sceneCB);
    }

    UINT stride = sizeof(DirectX::XMFLOAT3);
    UINT offset = 0;
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); // Shapeは全てLine描画

    auto DrawInstanced = [&](Mesh& mesh, std::vector<InstanceData> ShapeDrawList::* memberPtr) {
        dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);

        std::lock_guard<std::mutex> lock(_listMutex);

        for (auto& listPtr : _drawLists) {
            std::vector<InstanceData>& instances = (*listPtr).*memberPtr;
            if (instances.empty()) continue;

            size_t totalCount = instances.size();

            size_t drawOffset = 0;
            while (drawOffset < totalCount) {
                size_t drawCount = (std::min)(totalCount - drawOffset, InstanceBuffer::MAX_INSTANCES);
                std::vector<InstanceData> chunk(instances.begin() + drawOffset, instances.begin() + drawOffset + drawCount);

                _instanceBuffer.Write(dc, chunk);
                _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

                // インデックスバッファ無しで描画
                dc->DrawInstanced(mesh.vertexCount, static_cast<UINT>(drawCount), 0, 0);

                drawOffset += drawCount;
            }
            instances.clear();
        }
        };

    DrawInstanced(boxMesh, &ShapeDrawList::boxes);
    DrawInstanced(sphereMesh, &ShapeDrawList::spheres);
    DrawInstanced(halfSphereMesh, &ShapeDrawList::halfSpheres);
    DrawInstanced(cylinderMesh, &ShapeDrawList::cylinders);
    DrawInstanced(boneMesh, &ShapeDrawList::bones);
    DrawInstanced(lineMesh, &ShapeDrawList::lines); // デバッグで数万本出ても全く落ちない
    // =========================================================================
      // ★ここから下を丸ごと書き換えます（面を描く直前の最強ステート適用）
      // =========================================================================

      // 1. 他の描画に影響を与えないよう、現在のステートを退避
    Microsoft::WRL::ComPtr<ID3D11BlendState> oldBlendState;
    float oldBlendFactor[4];
    UINT oldSampleMask;
    dc->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> oldRasterizerState;
    dc->RSGetState(&oldRasterizerState);

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDepthStateSolid;
    UINT oldStencilRefSolid;
    dc->OMGetDepthStencilState(&oldDepthStateSolid, &oldStencilRefSolid);

    // 2. 半透明 ＆ 両面描画ステートを適用
    dc->OMSetBlendState(m_transparentBlendState.Get(), nullptr, 0xFFFFFFFF);
    dc->RSSetState(m_cullNoneRasterizerState.Get());


    dc->OMSetDepthStencilState(m_depthTestNoWriteState.Get(), 0); // テストはするが書き込みOFF
    

    // 4. トポロジを面に変更して、いざ描画！
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DrawInstanced(triangleFaceMesh, &ShapeDrawList::solidTriangles);

    // 5. 描画が終わったら、退避しておいた元のステートを完全に復元する
    dc->OMSetBlendState(oldBlendState.Get(), oldBlendFactor, oldSampleMask);
    dc->RSSetState(oldRasterizerState.Get());
    dc->OMSetDepthStencilState(oldDepthStateSolid.Get(), oldStencilRefSolid);

    // =========================================================================

    _instanceBuffer.Unbind(dc, SLOT_SRV_INSTANCE);

    // ステートクリア
    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);

    // 描画が終わったら、保存しておいた元の深度ステートに戻す（超重要！）
    dc->OMSetDepthStencilState(oldDepthState.Get(), oldStencilRef);
}