#ifndef SHADOWMAP_HLSLI
#define SHADOWMAP_HLSLI

#include "Engine/Graphics/Shader/ShaderResources.h"


// --- データ定義 ---
cbuffer CbShadow : register(REG_CB(SLOT_CB_SHADOW))
{
    row_major float4x4 light_view_projection[3]; // 3層の行列
    float4 cascadeSplits;
    float4 shadow_color;
    
    float shadow_attenuation;
    float shadow_bias;
    float normal_bias; // ★新規: 法線オフセットの強さ
    int currentCascadeIndex; // 書き込み用
    
    float3 light_direction; // ★新規: 光源の方向 (ワールド空間)
    float shadow_padding;
};


Texture2DArray ShadowMap : register(REG_SRV(SLOT_SRV_SHADOW));
SamplerComparisonState ShadowSampler : register(REG_SMP(SLOT_SMP_SHADOW));

// --- 関数定義 ---

// 1つのカスケード層から影をサンプリングする専用関数
float SampleCascade(float4 worldPos, int cascadeIndex)
{
    float4 shadowPos = mul(worldPos, light_view_projection[cascadeIndex]);
    float3 shadow_texcoord = shadowPos.xyz / shadowPos.w;
    
    // UV座標の変換
    shadow_texcoord.x = shadow_texcoord.x * 0.5f + 0.5f;
    shadow_texcoord.y = shadow_texcoord.y * -0.5f + 0.5f;

    // 範囲外判定
    if (any(shadow_texcoord.xy < 0.0f) || any(shadow_texcoord.xy > 1.0f) || shadow_texcoord.z < 0.0f || shadow_texcoord.z > 1.0f)
        return 1.0f;

    float depth = shadow_texcoord.z - shadow_bias;
    float shadowWeight = 0.0f;
    
    uint width, height, elements;
    ShadowMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0f / float2(width, height);
    
    // 3x3 PCFサンプリング
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            shadowWeight += ShadowMap.SampleCmpLevelZero(
                ShadowSampler, float3(shadow_texcoord.xy + offset, cascadeIndex), depth
            );
        }
    }
    return shadowWeight / 9.0f;
}


// 影の重み (0.0=影, 1.0=日向) を計算し、境界でブレンドするメイン関数
float CalculateShadowWeight(float4 worldPos, float viewDepth)
{
    int cascadeIndex = 0;
    int nextCascadeIndex = 0;
    float blendFactor = 0.0f;
    
    // ブレンドを開始する「境界の手前何メートルか」
    // (数値を大きくするとグラデーションが広がる)
    const float BLEND_BAND = 5.0f;

    if (viewDepth < cascadeSplits.x)
    {
        cascadeIndex = 0;
        // 次の層(Cascade 1)への距離を測る
        float distToNext = cascadeSplits.x - viewDepth;
        if (distToNext < BLEND_BAND)
        {
            nextCascadeIndex = 1;
            // 距離が近いほど、blendFactor が 1.0 に近づく
            blendFactor = 1.0f - (distToNext / BLEND_BAND);
        }
    }
    else if (viewDepth < cascadeSplits.y)
    {
        cascadeIndex = 1;
        // 次の層(Cascade 2)への距離を測る
        float distToNext = cascadeSplits.y - viewDepth;
        if (distToNext < BLEND_BAND)
        {
            nextCascadeIndex = 2;
            blendFactor = 1.0f - (distToNext / BLEND_BAND);
        }
    }
    else
    {
        cascadeIndex = 2; // 最終層はブレンド先がない
    }

    // 1. 現在の層で影をサンプリング（9回）
    float shadowWeight = SampleCascade(worldPos, cascadeIndex);

    // 2. 境界付近（blendFactor > 0）なら、次の層もサンプリングしてブレンドする
    if (blendFactor > 0.0f)
    {
        float nextShadowWeight = SampleCascade(worldPos, nextCascadeIndex); // さらに9回
        shadowWeight = lerp(shadowWeight, nextShadowWeight, blendFactor);
    }

    return shadowWeight;
}

// 最終的な色に影を適用する関数
float3 ApplyShadowColor(float3 finalColor, float shadowWeight)
{
    // shadow_color.a (例: 0.5) を「影の濃さ」として使用する
    // 日向(weight=1.0)なら 1.0倍。日陰(weight=0.0)なら 1.0 - 0.5 = 0.5倍の明るさになる
    float intensity = lerp(1.0f - shadow_color.a, 1.0f, shadowWeight);
    return finalColor * intensity;
}

#endif