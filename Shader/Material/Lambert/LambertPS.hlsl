#include "Shader/Common/Common.hlsli"
#include "Shader/Common/Lighting.hlsli"

cbuffer CbMaterial : register(REG_CB(SLOT_CB_MATERIAL))
{
    float4 materialColor;
};

Texture2D DiffuseMap : register(REG_SRV(SLOT_SRV_MAT_TEX0));
SamplerState LinearSampler : register(REG_SMP(SLOT_SMP_LINEAR));

float4 main(VS_OUT pin) : SV_TARGET
{
	// 1. ベースカラーの取得
    float4 texColor = DiffuseMap.Sample(LinearSampler, pin.texcoord);
    float4 baseColor = texColor * materialColor;
    
    // 透明なら早めに処理を打ち切る (最適化)
    if (baseColor.a < 0.01f)
        discard;
    
    // =========================================================
    // 2. 表面情報（SurfaceInfo）の構築
    // =========================================================
    SurfaceInfo surf;
    surf.pos = pin.position.xyz; // ワールド座標
    surf.normal = normalize(pin.normal);
    surf.viewDir = normalize(cameraPosition.xyz - pin.position.xyz);
    surf.color = baseColor.rgb;
    surf.alpha = baseColor.a;
    
    // =========================================================
    // 3. ECSライトシステム(CbLight)で全ライトを一括計算！
    // =========================================================
    float viewDepth = length(cameraPosition.xyz - pin.position.xyz);
    float shadowWeight = CalculateShadowWeight(pin.position, viewDepth);
    
    // 平行光源、点光源、スポットライトの全ての影響が計算されて返ってくる
    LightResult lit = CalculateAllLights(surf, shadowWeight);
    
    // =========================================================
    // 4. 環境光（半球ライティング）の計算
    // =========================================================
    // CbLight (gDirLight) が持っている空の色と地面の色を使って、
    // 真っ暗な日陰でも美しく自然な明るさを確保する
    float3 ambient = CalcHemiSphereLight(surf.normal, float3(0, 1, 0),
                                         gDirLight.gSkyColor.rgb, gDirLight.gGroundColor.rgb,
                                         float4(1, 1, 1, 1));
                                         
    // 5. 最終カラー合成 (Diffuse + Specular + Ambient)
    float3 finalColor = lit.diffuse + lit.specular + (baseColor.rgb * ambient);
    
    // 6. フォグの適用（距離フォグと高さフォグ）
    finalColor = ApplyHeightFog(surf.pos, cameraPosition.xyz, finalColor, LinearSampler, surf.normal);
    
    return float4(finalColor, surf.alpha);
}