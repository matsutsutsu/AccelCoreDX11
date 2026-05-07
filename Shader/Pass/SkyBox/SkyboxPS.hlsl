#include "Engine/Graphics/Shader/ShaderResources.h"

// IBLですでにセットされている s1 を流用する超絶スマートな設計
TextureCube SkyboxMap : register(REG_SRV(SLOT_SRV_SKYBOX_BG));
SamplerState LinearSampler : register(REG_SMP(SLOT_SMP_LINEAR));

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float3 rayDir : TEXCOORD;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    // Mipレベル強制(SampleLevel)をやめ、通常のSampleを使用する
    // これによりカメラの解像度に合わせた最適なミップマップが自動選択され、
    // ジャギー（ピクセルのチラつき）のない最高品質の空が描画されます。
    float3 envColor = SkyboxMap.Sample(LinearSampler, normalize(pin.rayDir)).rgb;
    
    
    return float4(envColor, 1.0f);
}