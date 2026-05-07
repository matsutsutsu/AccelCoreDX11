#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "Shader/Common/Scene.hlsli"
#include "Shader/Common/Lighting.hlsli"
#include "Shader/Pass/ShadowMap/ShadowMap.hlsli"


// プロジェクトで使う「最大公約数」的な頂点出力構造体
struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float4 position : POSITION;
    float4 tangent : TANGENT;
    float4 shadowPos : SHADOW_POS;
    
    // 個体ごとの汎用パラメータを受け渡す
    float4 customParams : CUSTOM_PARAMS;
};


// 共通フォグ計算関数
// ピクセルシェーダーから呼び出して使います
// Common.hlsli

// 引数に float3 normal を追加
float3 ApplyHeightFog(float3 pixelPos, float3 camPos, float3 baseColor, SamplerState smp, float3 normal)
{
    // 1. 距離フォグ
    float dist = length(pixelPos - gFogCenter.xyz);
    float distFog = saturate((dist - gFogParams.x) / (gFogParams.y - gFogParams.x));

    // 2. 高さフォグ
    float heightFog = exp(-(pixelPos.y - gFogParams.z) * gFogParams.w);
    heightFog = saturate(heightFog);

    float rawFogDensity = distFog * heightFog; // 乗算(マスク)か max かは調整次第ですが、乗算推奨

    // 3. ノイズ計算 (Noise Modulation)
    if (gFogNoiseParams.y > 0.001f)
    {
        float2 uv = pixelPos.xz * gFogNoiseParams.x;
        uv += float2(gFogNoiseParams.z, gFogNoiseParams.w);
        float noise = gFogNoiseMap.Sample(smp, uv).r;

        // 壁のノイズを消す 
        // normal.y (上向き成分) の絶対値を見る
        // 1.0 = 床/天井, 0.0 = 垂直な壁
        // 壁に近いほど係数を 0 にしてノイズを無効化する
        float wallMask = abs(normal.y);
        
        // 調整用: コントラストを付けて、少し傾いた坂道でもノイズが出るようにする
        wallMask = pow(wallMask, 2.0f); // 2乗～4乗くらいで調整
        
        // ノイズ強度に wallMask を掛ける
        // これで「壁」は noiseStrength が 0 になり、普通の白いフォグになる
        float erosionStrength = gFogNoiseParams.y * wallMask;
        
        // 浸食計算
        float erodedFog = (rawFogDensity - noise * erosionStrength) / (1.0001f - erosionStrength);
        rawFogDensity = saturate(erodedFog);
    }

    return lerp(baseColor, gFogColor.rgb, rawFogDensity * gFogColor.a);
}

#endif // COMMON_HLSLI