// Shader/Vertex/BasicVS.hlsl
#include "Shader/Common/Common.hlsli"
#include "Shader/Common/Transform.hlsli"

VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    uint instanceID : SV_InstanceID
)
{
    VS_OUT vout = (VS_OUT) 0;
    float4 worldPos;

    // 1. 位置のみのオーバーロード関数が自動的に呼ばれる
    CalculateWorldTransform(
        position, boneWeights, boneIndices, instanceID, worldPos
    );

    // 2. カメラ空間への投影
    vout.vertex = mul(worldPos, viewProjection);
    vout.texcoord = texcoord;

    return vout;
}