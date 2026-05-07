#include "JoltDebugRenderer.h"

void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    if (!m_shapeRenderer) return;

    DirectX::XMFLOAT3 from = {inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()};
    DirectX::XMFLOAT3 to   = {inTo.GetX(), inTo.GetY(), inTo.GetZ()};

    // ★修正: 確実な手動計算に戻す
    DirectX::XMFLOAT4 color = {
        inColor.r / 255.0f, inColor.g / 255.0f, inColor.b / 255.0f, inColor.a / 255.0f};

    m_shapeRenderer->DrawLine(from, to, color);
}

void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1,
    JPH::RVec3Arg                                  inV2,
    JPH::RVec3Arg                                  inV3,
    JPH::ColorArg                                  inColor,
    JPH::DebugRenderer::ECastShadow                inCastShadow)
{
    if (!m_shapeRenderer) return;

    DirectX::XMFLOAT3 v1 = {inV1.GetX(), inV1.GetY(), inV1.GetZ()};
    DirectX::XMFLOAT3 v2 = {inV2.GetX(), inV2.GetY(), inV2.GetZ()};
    DirectX::XMFLOAT3 v3 = {inV3.GetX(), inV3.GetY(), inV3.GetZ()};

    // ★修正: 同様に確実な手動計算
    DirectX::XMFLOAT4 color = {
        inColor.r / 255.0f, inColor.g / 255.0f, inColor.b / 255.0f, inColor.a / 255.0f};

    m_shapeRenderer->DrawTriangle(v1, v2, v3, color);
}