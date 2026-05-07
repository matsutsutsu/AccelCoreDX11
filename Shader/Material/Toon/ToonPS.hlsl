// Shader/Material/Toon/ToonPS.hlsl
#include "Shader/Common/Common.hlsli"
#include "Shader/Common/ToonLighting.hlsli"
#include "Shader/Common/MaterialEffect.hlsli"

cbuffer CbToonMaterial : register(REG_CB(SLOT_CB_MATERIAL))
{
    float4 materialColor;
    float4 gRimColor;
    float gRimPower;
    float gRimIntensity;
    float2 gToonPadding;
};

Texture2D DiffuseMap : register(REG_SRV(SLOT_SRV_MAT_TEX0));
Texture2D NormalMap : register(REG_SRV(SLOT_SRV_MAT_TEX1));
Texture2D ToonRampMap : register(REG_SRV(SLOT_SRV_MAT_TEX2));
Texture2D DissolveNoiseMap : register(REG_SRV(SLOT_SRV_GLOBAL_NOISE));
SamplerState LinearSampler : register(REG_SMP(SLOT_SMP_LINEAR));

float4 main(VS_OUT pin) : SV_TARGET
{
    // ① 下ごしらえ
    float dissolveThreshold = pin.customParams.x;
    ApplyDissolveClip(dissolveThreshold, pin.texcoord, DissolveNoiseMap, LinearSampler);

    SurfaceInfo surf;
    float4 sampledColor = DiffuseMap.Sample(LinearSampler, pin.texcoord) * materialColor;
    surf.color = sampledColor.rgb;
    surf.alpha = sampledColor.a;
    surf.pos = pin.position.xyz;
    surf.viewDir = normalize(cameraPosition.xyz - surf.pos);
    surf.normal = CalculateWorldNormal(pin.normal, pin.tangent, pin.texcoord, NormalMap, LinearSampler);

    // ② 光の調理 (Toon専用)
    float viewDepth = length(cameraPosition.xyz - pin.position.xyz);
    float shadowWeight = CalculateShadowWeight(pin.position, viewDepth);
    LightResult lightRes = (LightResult) 0;
    
    CalcToonDirectionalLight(surf, shadowWeight, ToonRampMap, LinearSampler, lightRes);
    
    float hemiWeight = surf.normal.y * 0.5f + 0.5f;
    float3 ambient = lerp(gDirLight.gGroundColor.rgb * gDirLight.gGroundColor.a, gDirLight.gSkyColor.rgb * gDirLight.gSkyColor.a, hemiWeight);
    
    // Toon固有のリムライト
    float rimFactor = pow(1.0f - saturate(dot(surf.viewDir, surf.normal)), gRimPower);
    float3 toonRimLight = gRimColor.rgb * rimFactor * gRimIntensity * shadowWeight;

    float3 finalColor = surf.color * (lightRes.diffuse + ambient) + toonRimLight;

    // ③ 飾り付け
    finalColor = ApplyShadowColor(finalColor, shadowWeight);
    // ※ 共通のリムライトはToon専用リムがあるためここでは呼ばない
    finalColor = ApplyHeightFog(surf.pos, cameraPosition.xyz, finalColor, LinearSampler, surf.normal);
    finalColor = ApplyDissolveEdge(finalColor, dissolveThreshold, pin.texcoord, DissolveNoiseMap, LinearSampler);
    
    return float4(finalColor, surf.alpha);
}