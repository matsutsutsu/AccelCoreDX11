// Shader/Common/Transform.hlsli
#ifndef TRANSFORM_HLSLI
#define TRANSFORM_HLSLI

#include "Shader/Common/Skinning.hlsli"
#include "Shader/Common/Instancing.hlsli"

// ==========================================================
// 1. 位置・法線・タンジェント の変換 (Phong, PBR, Toon用)
// ==========================================================
void CalculateWorldTransform(
    in float4 localPos, in float3 localNormal, in float4 localTangent,
    in float4 boneWeights, in uint4 boneIndices, in uint instanceID,
    out float4 outWorldPos, out float3 outWorldNormal, out float3 outWorldTangent)
{
    float weightSum = dot(boneWeights, float4(1, 1, 1, 1));
    if (weightSum > 0.5f)
    {
        // スキンメッシュ
        outWorldPos = SkinningPosition(localPos, boneWeights, boneIndices);
        outWorldNormal = SkinningVector(localNormal, boneWeights, boneIndices);
        outWorldTangent = SkinningVector(localTangent.xyz, boneWeights, boneIndices);
    }
    else
    {
        // インスタンシング（静的メッシュ）
        float4x4 world = GetInstanceWorld(instanceID);
        outWorldPos = mul(localPos, world);
        outWorldNormal = normalize(mul(float4(localNormal, 0), world).xyz);
        outWorldTangent = normalize(mul(float4(localTangent.xyz, 0), world).xyz);
    }
}

// ==========================================================
// 2. 位置・法線 のみの変換 (Lambert, Outline用)
// ==========================================================
void CalculateWorldTransform(
    in float4 localPos, in float3 localNormal,
    in float4 boneWeights, in uint4 boneIndices, in uint instanceID,
    out float4 outWorldPos, out float3 outWorldNormal)
{
    float weightSum = dot(boneWeights, float4(1, 1, 1, 1));
    if (weightSum > 0.5f)
    {
        outWorldPos = SkinningPosition(localPos, boneWeights, boneIndices);
        outWorldNormal = SkinningVector(localNormal, boneWeights, boneIndices);
    }
    else
    {
        float4x4 world = GetInstanceWorld(instanceID);
        outWorldPos = mul(localPos, world);
        outWorldNormal = normalize(mul(float4(localNormal, 0), world).xyz);
    }
}

// ==========================================================
// 3. 位置 のみの変換 (Basic, ShadowMap用)
// ==========================================================
void CalculateWorldTransform(
    in float4 localPos,
    in float4 boneWeights, in uint4 boneIndices, in uint instanceID,
    out float4 outWorldPos)
{
    float weightSum = dot(boneWeights, float4(1, 1, 1, 1));
    if (weightSum > 0.5f)
    {
        outWorldPos = SkinningPosition(localPos, boneWeights, boneIndices);
    }
    else
    {
        float4x4 world = GetInstanceWorld(instanceID);
        outWorldPos = mul(localPos, world);
    }
}

#endif // TRANSFORM_HLSLI