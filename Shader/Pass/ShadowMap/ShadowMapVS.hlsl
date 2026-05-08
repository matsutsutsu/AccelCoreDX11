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
    float3 worldNormal;
    
    // 修正点: Transform.hlsli に定義されている「位置と法線」の両方を出力するオーバーロードを使用する
    CalculateWorldTransform(position, normal, boneWeights, boneIndices, instanceID, worldPos, worldNormal);

    // 正規化（スケールがかかっている場合を考慮）
    worldNormal = normalize(worldNormal);

    // 光源に向かうベクトル (L) を作成し、内積 (NdotL) を計算
    // ※ light_direction が CbShadow に正しく追加・転送されているか確認すること
    float3 L = normalize(-light_direction);
    float NdotL = saturate(dot(worldNormal, L));

    // 法線オフセットバイアスを適用
    // 光が浅い角度 (NdotLが0に近い) ほど、押し出し量 (offsetScale) が最大化される
    float offsetScale = (1.0f - NdotL) * normal_bias;
    worldPos.xyz += worldNormal * offsetScale;

    // currentCascadeIndex を使って適切な行列を選択する
    return mul(worldPos, light_view_projection[currentCascadeIndex]);
}