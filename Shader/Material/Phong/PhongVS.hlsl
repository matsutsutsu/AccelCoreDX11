// Shader/Vertex/PhongVS.hlsl
#include "Shader/Common/Common.hlsli"
#include "Shader/Common/Transform.hlsli"


VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    float4 tangent : TANGENT,
    float3 normal : NORMAL,
    uint instanceID : SV_InstanceID
)
{
    VS_OUT vout = (VS_OUT) 0;
    
    // 1. ワールド空間への変換をファクトリー関数に委譲
    float3 worldTangent;
    CalculateWorldTransform(
        position, normal, tangent,
        boneWeights, boneIndices, instanceID,
        vout.position, vout.normal, worldTangent
    );
    vout.tangent = float4(worldTangent, tangent.w);

    // 2. カメラ・ライト空間への投影
    vout.vertex = mul(vout.position, viewProjection);

    
    // 3. その他パラメータのパススルー
    vout.texcoord = texcoord;
    vout.customParams = GetInstanceCustomParams(instanceID);
    
    return vout;
}