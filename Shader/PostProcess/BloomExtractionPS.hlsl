#include "Shader/PostProcess/Bloom.hlsli"

float4 main(VS_OUT input) : SV_TARGET
{
    // バイリニアサンプリングで色を取得（実質的な縮小）
    float4 color = gInputTexture.Sample(LinearSampler, input.texcoord);
    
    // 【対策1】負の値を防ぐ（NaNの温床になりやすい）
    color.rgb = max(0.0f, color.rgb);
    
    // 輝度（明るさ）を計算 (Rec.709係数)
    float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    
    // --- 閾値処理 (Soft Knee) ---
    // 完全に切るのではなく、境界を滑らかにする
    float knee = gThreshold * gSoftKnee;
    float soft = brightness - gThreshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 0.00001f);
    
    float contribution = max(soft, brightness - gThreshold);
    contribution /= max(brightness, 0.00001f);
    
    // 抽出された色
    return color * contribution * gIntensity;
}