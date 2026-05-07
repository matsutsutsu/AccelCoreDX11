// BloomUpsamplePS.hlsl
#include "Shader/PostProcess/Bloom.hlsli"

float4 main(VS_OUT input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float2 texel = gTexelSize; // 拡大フィルター半径（大きめに取るとボケが広がる）
    
    // 3x3 Tent Filter (9-tap)
    float3 d = gInputTexture.Sample(LinearSampler, uv + float2(-1, -1) * texel).rgb * 1.0;
    float3 e = gInputTexture.Sample(LinearSampler, uv + float2(0, -1) * texel).rgb * 2.0;
    float3 f = gInputTexture.Sample(LinearSampler, uv + float2(1, -1) * texel).rgb * 1.0;

    float3 g = gInputTexture.Sample(LinearSampler, uv + float2(-1, 0) * texel).rgb * 2.0;
    float3 h = gInputTexture.Sample(LinearSampler, uv + float2(0, 0) * texel).rgb * 4.0;
    float3 i = gInputTexture.Sample(LinearSampler, uv + float2(1, 0) * texel).rgb * 2.0;

    float3 j = gInputTexture.Sample(LinearSampler, uv + float2(-1, 1) * texel).rgb * 1.0;
    float3 k = gInputTexture.Sample(LinearSampler, uv + float2(0, 1) * texel).rgb * 2.0;
    float3 l = gInputTexture.Sample(LinearSampler, uv + float2(1, 1) * texel).rgb * 1.0;
    
    float3 blur = (d + e + f + g + h + i + j + k + l) / 16.0f;
    
    // 1つ上の解像度の画像（保存しておいたDownChainの結果）と加算合成
    float3 highRes = gHighResTexture.Sample(LinearSampler, uv).rgb;
    
    float3 result = blur + highRes;
    
    // 【重要】安全装置
    if (any(isnan(result)) || any(isinf(result)))
    {
        // ここでNaNが出ると黒い四角の原因になるため、必ず0を返す
        return float4(0, 0, 0, 0);
    }

    return float4(result, 1.0f);
}