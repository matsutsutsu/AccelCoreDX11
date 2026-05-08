// Shader/Material/Trail/TrailPS.hlsl
#include "Shader/Common/Common.hlsli"

// トレイルの模様（剣の軌跡っぽいテクスチャ。無ければ白いテクスチャなどを適用）
Texture2D TrailTexture : register(REG_SRV(SLOT_SRV_MAT_TEX0)); 
SamplerState LinearSampler : register(REG_SMP(SLOT_SMP_LINEAR));

// 頂点シェーダーから受け取る構造体
struct TRAIL_VS_OUT
{
    float4 vertex : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

float4 main(TRAIL_VS_OUT pin) : SV_TARGET
{
    // テクスチャの色をサンプリング
    float4 texColor = TrailTexture.Sample(LinearSampler, pin.uv);
    
    // 頂点カラー（CPUで計算した時間経過によるアルファ・フェードアウトを含む）を掛け合わせる
    float4 finalColor = texColor * pin.color;
    
    return finalColor;
}