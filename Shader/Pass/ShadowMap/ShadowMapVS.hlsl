#include "Shader/Common/Transform.hlsli"
#include "Shader/Pass/ShadowMap/ShadowMap.hlsli"

float4 main(
    float4 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 texcoord : TEXCOORD,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    uint instanceID : SV_InstanceID
) : SV_POSITION
{
    float4 worldPos;
    CalculateWorldTransform(position, boneWeights, boneIndices, instanceID, worldPos);

    // currentCascadeIndex を使って適切な行列を選択する
    return mul(worldPos, light_view_projection[currentCascadeIndex]);
}