// Shader/Common/ToonLighting.hlsli
#ifndef TOON_LIGHTING_HLSLI
#define TOON_LIGHTING_HLSLI

#include "Shader/Common/Lighting.hlsli"

// トゥーン用ランプテクスチャを用いた平行光源の計算
void CalcToonDirectionalLight(SurfaceInfo surf, float shadowWeight, Texture2D rampMap, SamplerState smp, inout LightResult res)
{
    float3 L = normalize(gDirLight.direction);
    float halfLambert = dot(surf.normal, -L) * 0.5f + 0.5f;
    
    // ランプテクスチャのサンプリング
    float3 ramp = rampMap.Sample(smp, float2(halfLambert, 0.5f)).rgb;
    res.diffuse += gDirLight.color * gDirLight.intensity * ramp * shadowWeight;
}

// ※ 今後 Toon の点光源・スポットライトを追加する場合もここに記述します

#endif // TOON_LIGHTING_HLSLI