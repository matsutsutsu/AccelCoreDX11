// BloomDownsamplePS.hlsl
#include "Shader/PostProcess/Bloom.hlsli"

float4 main(VS_OUT input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float2 texel = gTexelSize; // 入力テクスチャの1ピクセルサイズ

    // Dual Filter (13-tap)
    // -----------------------------------------------------
    // Center
    float3 c_center = gInputTexture.Sample(LinearSampler, uv).rgb;
    
    // Inner Box (4 samples)
    float3 c_inner = (
        gInputTexture.Sample(LinearSampler, uv + float2(-1, -1) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(1, -1) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(-1, 1) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(1, 1) * texel).rgb
    );

    // Outer Box (4 corners + 4 sides = 8 samples)
    float3 c_outer = (
        gInputTexture.Sample(LinearSampler, uv + float2(-2, -2) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(0, -2) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(2, -2) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(-2, 0) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(2, 0) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(-2, 2) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(0, 2) * texel).rgb +
        gInputTexture.Sample(LinearSampler, uv + float2(2, 2) * texel).rgb
    );

    // 加重平均
    // CenterはOuterに含まれる位置(0,0)と重複するため少し調整しても良いが、
    // ここでは標準的な Dual Filter のウェイトを使用
    // Inner(4) * 0.5 + Outer(8+Center) * 0.125
    // ※提示されたコードをベースに少し整理しましたが、元の計算式でもOKです
    
    // ユーザー様の元の計算式を採用する場合:
    float3 color = 0.0f;
    color += c_inner * 0.5f; // Box (4隅)
    color += (c_outer + c_center) * 0.125f; // Box (中心周辺)
    
    color /= 4.0f; // 正規化

    // 【重要】安全装置
    if (any(isnan(color)) || any(isinf(color)))
    {
        return float4(0, 0, 0, 0);
    }

    return float4(color, 1.0f);
}