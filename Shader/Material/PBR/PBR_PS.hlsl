// Shader/Material/PBR/PBR_PS.hlsl
#include "Shader/Common/Common.hlsli"
#include "Shader/Common/PBRLighting.hlsli"

// ==============================================================================
// 定数バッファ & リソース
// ==============================================================================
cbuffer CbMesh : register(REG_CB(SLOT_CB_MATERIAL))
{
    float4 baseColor;
    float3 emissiveColor;
    float metalness;
    float roughness;
    float occlusionStrength;
    
    // C++側から送られてくる「テクスチャの有無」フラグ
    float useAlbedoMap;
    float useNormalMap;
    float useMetalRoughMap;
    float useOcclusionMap;
};

Texture2D AlbedoMap : register(REG_SRV(SLOT_SRV_MAT_TEX0));
Texture2D NormalMap : register(REG_SRV(SLOT_SRV_MAT_TEX1));
Texture2D MetalRoughMap : register(REG_SRV(SLOT_SRV_MAT_TEX2));
Texture2D EmissiveMap : register(REG_SRV(SLOT_SRV_PARTICLE_BUF)); 

SamplerState LinearSampler : register(REG_SMP(SLOT_SMP_LINEAR));

// ==============================================================================
// メイン関数
// ==============================================================================
float4 main(VS_OUT pin) : SV_TARGET
{
    PBRSurfaceInfo surf;
    surf.pos = pin.position.xyz;
    surf.viewDir = normalize(cameraPosition.xyz - surf.pos);

    // ------------------------------------------------------------
    // 1. テクスチャのサンプリングとフォールバック処理
    // ------------------------------------------------------------
    
    // ノーマルマップ（無い場合は頂点法線をそのまま使用）
    if (useNormalMap > 0.5f)
    {
        surf.normal = CalculateWorldNormal(pin.normal, pin.tangent, pin.texcoord, NormalMap, LinearSampler);
    }
    else
    {
        surf.normal = normalize(pin.normal);
    }

    // アルベドマップ（無い場合は白として扱う）
    float4 albedoTex = useAlbedoMap > 0.5f ? AlbedoMap.Sample(LinearSampler, pin.texcoord) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    surf.albedo = albedoTex.rgb * baseColor.rgb;
    float alpha = albedoTex.a * baseColor.a;

    // メタル・ラフネス・オクルージョンマップ（無い場合は白として扱う）
    // R: AO, G: Roughness, B: Metalness
    float4 mrSample = useMetalRoughMap > 0.5f ? MetalRoughMap.Sample(LinearSampler, pin.texcoord) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    surf.metalness = metalness * mrSample.b;
    surf.roughness = roughness * mrSample.g;
    float rawAO = useOcclusionMap > 0.5f ? mrSample.r : 1.0f;
    surf.ao = lerp(1.0f, rawAO, occlusionStrength);

    // F0の決定 (非金属は0.04、金属は自身のアルベド色)
    surf.f0 = lerp(float3(0.04f, 0.04f, 0.04f), surf.albedo, surf.metalness);

    // ------------------------------------------------------------
    // 2. 光の計算（物理演算）
    // ------------------------------------------------------------
    float viewDepth = length(cameraPosition.xyz - pin.position.xyz);
    float shadowWeight = CalculateShadowWeight(pin.position, viewDepth);

    // 全光源からの直接光を計算
    LightResult light = CalculateAllLightsPBR(surf, shadowWeight);

    // IBLによる環境光（間接光）を計算
    float3 ambientIBL = CalculateIBL(surf);
    
    // ==============================================================================
    // マクロオクルージョン（影の中のIBLを減衰させて、影をクッキリさせる）
    // shadowWeight が 0.0 (影) のときは IBLを 20% (0.2) に落とし、
    // shadowWeight が 1.0 (日向) のときは IBLを 100% (1.0) にする。
    // ==============================================================================
    float skyOcclusion = lerp(0.2f, 1.0f, shadowWeight);
    ambientIBL *= skyOcclusion;
    
    // ------------------------------------------------------------
    // 3. 最終合成とエフェクト
    // ------------------------------------------------------------
    float3 emissive = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveColor;
    
    // 直接光 ＋ 環境光 ＋ 発光
    float3 finalColor = ambientIBL + light.diffuse + light.specular + emissive;
    
    // リムライト
    if (gFogRimParams.y > 0.0f)
    {
        float NdotV = saturate(dot(surf.normal, surf.viewDir));
        float rimFactor = pow(1.0f - NdotV, gFogRimParams.x);
        float curvature = length(fwidth(surf.normal));
        float flatMask = saturate(curvature * 100.0f);
        float floorMask = 1.0f - step(0.9f, surf.normal.y);
        
        finalColor += rimFactor * gFogRimColor.rgb * gFogRimParams.y * flatMask * floorMask;
    }
    
    // フォグ
    finalColor = ApplyHeightFog(surf.pos, cameraPosition.xyz, finalColor, LinearSampler, surf.normal);

    return float4(finalColor, alpha);
}