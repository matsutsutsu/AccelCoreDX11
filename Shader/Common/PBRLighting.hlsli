// Shader/Common/PBRLighting.hlsli
#ifndef PBR_LIGHTING_HLSLI
#define PBR_LIGHTING_HLSLI

#include "Shader/Common/Lighting.hlsli"
#include "Engine/Graphics/Shader/ShaderResources.h"

static const float PI = 3.14159265359f;

// ==============================================================================
// PBR用の表面情報構造体
// ==============================================================================
struct PBRSurfaceInfo
{
    float3 pos;
    float3 normal;
    float3 viewDir;
    float3 albedo;
    float roughness;
    float metalness;
    float3 f0; // 垂直入射時の反射率
    float ao; // アンビエントオクルージョン
};

// ==============================================================================
// Cook-Torrance BRDF 物理演算関数群
// ==============================================================================

// 1. フレネル反射 (Schlickの近似式)
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // saturateを追加しNaN(黒落ち)を防止
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// IBL用のラフネスを考慮したフレネル反射
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 r = float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness);
    return F0 + (max(r, F0) - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// 2. マイクロファセット分布 (GGX)
float DistributionGGX(float3 N, float3 H, float rough)
{
    float a = rough * rough;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0000001f);
}

// 3. 幾何減衰 (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float rough)
{
    float r = (rough + 1.0f);
    float k = (r * r) / 8.0f;

    float denom = NdotV * (1.0f - k) + k;
    return NdotV / denom;
}

// 幾何減衰 (Smithの合成)
float GeometrySmith(float3 N, float3 V, float3 L, float rough)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    
    float ggx2 = GeometrySchlickGGX(NdotV, rough);
    float ggx1 = GeometrySchlickGGX(NdotL, rough);
    
    return ggx1 * ggx2;
}

// ==============================================================================
// 直接光（Direct Lighting）の計算
// ==============================================================================

// 単一のライトに対するPBR計算
LightResult CalcPBRLight(PBRSurfaceInfo surf, float3 L, float3 radiance)
{
    LightResult res = { float3(0, 0, 0), float3(0, 0, 0) };

    float3 H = normalize(surf.viewDir + L);
    float NdotL = max(dot(surf.normal, L), 0.0f);
    float NdotV = max(dot(surf.normal, surf.viewDir), 0.0f);

    if (NdotL > 0.0f)
    {
        // クック・トランスBRDFの各項を計算
        float NDF = DistributionGGX(surf.normal, H, surf.roughness);
        float G = GeometrySmith(surf.normal, surf.viewDir, L, surf.roughness);
        float3 F = FresnelSchlick(max(dot(H, surf.viewDir), 0.0f), surf.f0);

        // スペキュラ（鏡面反射）項
        float3 nominator = NDF * G * F;
        float denominator = 4.0f * NdotV * NdotL + 0.001f;
        res.specular = (nominator / denominator) * radiance * NdotL;

        // ディフューズ（拡散反射）項
        float3 kD = (float3(1.0f, 1.0f, 1.0f) - F) * (1.0f - surf.metalness);
        res.diffuse = (kD * surf.albedo / PI) * radiance * NdotL;
    }
    return res;
}

// 全光源の統合計算
LightResult CalculateAllLightsPBR(PBRSurfaceInfo surf, float shadowWeight)
{
    LightResult totalRes = { float3(0, 0, 0), float3(0, 0, 0) };

    // 1. 平行光源
    {
        float3 L = normalize(-gDirLight.direction);
        float3 radiance = gDirLight.color * gDirLight.intensity;
        LightResult res = CalcPBRLight(surf, L, radiance);
        
        totalRes.diffuse += res.diffuse * shadowWeight;
        totalRes.specular += res.specular * shadowWeight;
    }

    // 2. 点光源
    for (int i = 0; i < gPointLightCount; ++i)
    {
        if (gPointLights[i].active < 0.5f)
            continue;

        float3 L = gPointLights[i].position - surf.pos;
        float d = length(L);
        L /= d;

        if (d < gPointLights[i].range)
        {
            float atten = 1.0f / (gPointLights[i].constantAttenuation + gPointLights[i].linearAttenuation * d + gPointLights[i].quadraticAttenuation * (d * d));
            float fade = saturate(1.0f - d / gPointLights[i].range);
            float3 radiance = gPointLights[i].color * gPointLights[i].intensity * atten * fade;
            
            LightResult res = CalcPBRLight(surf, L, radiance);
            totalRes.diffuse += res.diffuse;
            totalRes.specular += res.specular;
        }
    }

    // 3. スポットライト
    for (int j = 0; j < gSpotLightCount; ++j)
    {
        if (gSpotLights[j].active < 0.5f)
            continue;

        float3 L = gSpotLights[j].position - surf.pos;
        float d = length(L);
        L /= d;

        if (d < gSpotLights[j].range)
        {
            float cosAngle = dot(normalize(gSpotLights[j].direction), -L);
            float angleAtten = saturate((cosAngle - gSpotLights[j].outerCos) / (gSpotLights[j].innerCos - gSpotLights[j].outerCos));
            float distAtten = 1.0f / (gSpotLights[j].constantAttenuation + gSpotLights[j].linearAttenuation * d + gSpotLights[j].quadraticAttenuation * (d * d));
            float fade = saturate(1.0f - d / gSpotLights[j].range);
            
            float3 radiance = gSpotLights[j].color * gSpotLights[j].intensity * angleAtten * distAtten * fade;
            
            LightResult res = CalcPBRLight(surf, L, radiance);
            totalRes.diffuse += res.diffuse;
            totalRes.specular += res.specular;
        }
    }

    return totalRes;
}

// ==============================================================================
// 環境光（Image-Based Lighting）の計算
// ==============================================================================

TextureCube gIrradianceMap : register(REG_SRV(SLOT_SRV_IBL_IRRADIANCE));
TextureCube gPrefilterMap : register(REG_SRV(SLOT_SRV_IBL_PREFILTER));
Texture2D gBRDFLUT : register(REG_SRV(SLOT_SRV_IBL_BRDFLUT));
SamplerState gSamplerIBL : register(REG_SMP(SLOT_SMP_IBL));

float3 CalculateIBL(PBRSurfaceInfo surf)
{
    float NdotV = saturate(dot(surf.normal, surf.viewDir));
    float3 R = reflect(-surf.viewDir, surf.normal); // 視線の反射ベクトル

    // フレネル（粗さを考慮）
    float3 F = FresnelSchlickRoughness(NdotV, surf.f0, surf.roughness);

    // 1. ディフューズ環境光 (Irradiance)
    float3 kD = (float3(1.0f, 1.0f, 1.0f) - F) * (1.0f - surf.metalness);
    float3 irradiance = gIrradianceMap.Sample(gSamplerIBL, surf.normal).rgb;
    float3 diffuseIBL = irradiance * surf.albedo * kD;

    // 2. スペキュラ環境光 (Prefilter + BRDF LUT)
    const float MAX_REFLECTION_LOD = 4.0f; // ミップレベル最大値
    float3 prefilteredColor = gPrefilterMap.SampleLevel(gSamplerIBL, R, surf.roughness * MAX_REFLECTION_LOD).rgb;
    float2 envBRDF = gBRDFLUT.Sample(gSamplerIBL, float2(NdotV, surf.roughness)).rg;
    
    float3 specularIBL = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    // アンビエントオクルージョン(AO)を適用
    return (diffuseIBL + specularIBL) * surf.ao;
}

#endif // PBR_LIGHTING_HLSLI