#include "Shader/Common/Common.hlsli"
#include "Shader/Common/Transform.hlsli"

// 定数バッファの定義
cbuffer CbMesh : register(b0)
{
    float4 materialColor;
};
cbuffer CbOutline : register(b1)
{
    float4 gOutlineColor;
    float gOutlineSize;
    float3 gOutlinePad;
};

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
    float4 skinPos;
    float3 skinNormal;

    // 1. ワールド空間での座標と法線を取得
    CalculateWorldTransform(
        position, normal,
        boneWeights, boneIndices, instanceID,
        skinPos, skinNormal
    );

    // 2. 本処理：アウトラインの膨張処理 (ワールド空間)
    skinPos.xyz += normalize(skinNormal) * gOutlineSize;

    // 3. 座標変換と出力
    vout.vertex = mul(skinPos, viewProjection);
    vout.position = skinPos;
    vout.normal = skinNormal;
    vout.texcoord = texcoord;
    
    // 不要なデータは0埋め
    vout.tangent = float4(0, 0, 0, 0);
    vout.shadowPos = float4(0, 0, 0, 0);

    return vout;
}