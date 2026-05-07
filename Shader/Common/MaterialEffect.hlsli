// Shader/Common/MaterialEffect.hlsli
#ifndef MATERIAL_EFFECT_HLSLI
#define MATERIAL_EFFECT_HLSLI

#include "Shader/Common/Scene.hlsli"

// ==========================================================
// 1. ディゾルブ (消滅) 判定
// ==========================================================
void ApplyDissolveClip(float dissolveThreshold, float2 uv, Texture2D noiseMap, SamplerState smp)
{
    if (dissolveThreshold > 0.0f)
    {
        float noiseScale = 0.1f;
        float noiseValue = noiseMap.Sample(smp, uv * noiseScale).r;
        clip(noiseValue - dissolveThreshold);
    }
}

// ==========================================================
// 2. ディゾルブのエッジ発光
// ==========================================================
float3 ApplyDissolveEdge(float3 baseColor, float dissolveThreshold, float2 uv, Texture2D noiseMap, SamplerState smp)
{
    if (dissolveThreshold > 0.0f)
    {
        float noiseValue = noiseMap.Sample(smp, uv * 3.0f).r;
        float edgeWidth = 0.05f;
        if (noiseValue - dissolveThreshold < edgeWidth)
        {
            // 発光するエッジカラー
            return float3(3.0f, 1.0f, 0.2f);
        }
    }
    return baseColor;
}

// ==========================================================
// 3. リムライト (輪郭発光)
// ==========================================================
float3 ApplyRimLight(float3 baseColor, float3 normal, float3 viewDir)
{
    if (gFogRimParams.y <= 0.0f)
        return baseColor;

    float NdotV = saturate(dot(normal, viewDir));
    float rimFactor = pow(1.0f - NdotV, gFogRimParams.x);
    
    float curvature = length(fwidth(normal));
    float flatMask = saturate(curvature * 100.0f);
    float floorMask = 1.0f - step(0.9f, normal.y);
    
    float3 rimLight = rimFactor * gFogRimColor.rgb * gFogRimParams.y * flatMask * floorMask;
    return baseColor + rimLight;
}

// ==========================================================
// 4. ヒットフラッシュ (被ダメージ時の白点滅)
// ==========================================================
float3 ApplyHitFlash(float3 baseColor, float flashIntensity)
{
    return lerp(baseColor, float3(1.0, 1.0, 1.0), flashIntensity);
}

#endif // MATERIAL_EFFECT_HLSLI