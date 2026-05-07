#include "Shader/Common/Common.hlsli"
#include "Shader/Common/Transform.hlsli"




VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    float3 normal : NORMAL,
    uint instanceID : SV_InstanceID
)
{
    VS_OUT vout = (VS_OUT) 0;
    float4 worldPos;
    float3 worldNormal;

    // 1. オーバーロードによる 位置・法線 のみの変換
    CalculateWorldTransform(
        position, normal,
        boneWeights, boneIndices, instanceID,
        worldPos, worldNormal
    );
    
    vout.normal = worldNormal;

    // 2. カメラ空間への投影
    vout.vertex = mul(worldPos, viewProjection);
    vout.texcoord = texcoord;

    return vout;
}