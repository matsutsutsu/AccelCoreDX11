// Shader/Material/Dissolve/DissolvePS.hlsl

#include "Shader/Common/Common.hlsli"
#include "Engine/Graphics/Shader/ShaderResources.h"

// ==========================================================
// 定数バッファ (Material)
// ==========================================================
cbuffer CbMaterial : register(REG_CB(SLOT_CB_MATERIAL)) // b0
{
    float4 materialColor; // Basic/Lambertに合わせて変数名を統一
    
    // ディゾルブ専用パラメータ
    float dissolveThreshold; // 0.0(全く消えない) ～ 1.0(完全に消える)
    float edgeWidth; // 発光エッジの幅 (例: 0.05)
    float2 padding; // 16バイトアライメント用の余白
    
    float4 edgeColor; // エッジの発光色
};

// ==========================================================
// テクスチャ＆サンプラ
// ==========================================================
Texture2D diffuseMap : register(REG_SRV(SLOT_SRV_MAT_TEX0)); // t0
Texture2D noiseMap : register(REG_SRV(SLOT_SRV_MAT_TEX2)); // t2 (ディゾルブ用ノイズテクスチャ)
SamplerState linearSampler : register(REG_SMP(SLOT_SMP_DEFAULT)); // s0 (Basic/Lambertの名称に合わせる)

// ==========================================================
// エントリポイント
// ==========================================================
// PSInputではなく、Common.hlsliで定義されている VS_OUT を使用する
float4 main(VS_OUT pin) : SV_TARGET
{
    // 1. ノイズマップから値をサンプリング (0.0 ～ 1.0)
    // pin.texcoord に修正
    float noiseVal = noiseMap.Sample(linearSampler, pin.texcoord).r;
    
    // 2. ディゾルブ（消滅）判定
    clip(noiseVal - dissolveThreshold);
    
    // 3. 通常のカラー計算 (Lambertライティングを適用)
    float4 color = diffuseMap.Sample(linearSampler, pin.texcoord) * materialColor;
    
    float3 N = normalize(pin.normal);
    float3 L = normalize(-lightDirection.xyz);
    float power = max(0, dot(L, N));
    power = power * 0.7 + 0.3f;
    color.rgb *= lightColor.rgb * power;
    
    // 4. エッジグロウ（境界線の発光）の計算
    float edgeFactor = 1.0 - saturate((noiseVal - dissolveThreshold) / edgeWidth);
    
    // 境界付近であれば、設定したエッジカラーを加算発光させる
    color.rgb += edgeColor.rgb * edgeFactor;
    
    return color;
}