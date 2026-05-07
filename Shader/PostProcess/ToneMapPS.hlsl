#include "Shader/PostProcess/Bloom.hlsli"


// シーン画像(HDR)
Texture2D gSceneTexture : register(REG_SRV(SLOT_SRV_MAT_TEX0));
// ブルーム画像
Texture2D gBloomTexture : register(REG_SRV(SLOT_SRV_MAT_TEX1));

// トーンマップ設定
cbuffer CbToneMap : register(REG_CB(SLOT_CB_TONEMAP))
{
    float gExposure; // 露出 (1.0)
    float3 ggPadding;
};

// ACESフィルム調トーンマップ（映画のような色合いになる）
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ToneMapPS.hlsl の main関数

float4 main(VS_OUT input) : SV_TARGET
{
    // 1. HDRキャンバスから「生の光エネルギー」をサンプリング
    float3 sceneColor = gSceneTexture.Sample(LinearSampler, input.texcoord).rgb;
    
    // 2. ボカされた光（Bloom）のエネルギーを「加算」する
    float3 bloomColor = gBloomTexture.Sample(LinearSampler, input.texcoord).rgb;
    float3 hdrColor = sceneColor + bloomColor;

    // 3. カメラレンズの「露出（Exposure）」を適用して、取り込む光の量を決定
    hdrColor *= gExposure;

    // 4. トーンマッピング (HDRの強烈な光を、白飛びさせずに SDR の 0.0〜1.0 へ滑らかに圧縮)
    float3 sdrColor = ACESFilm(hdrColor);

    // 5. ガンマ補正 (Linear -> sRGB空間への変換)
    // 液晶モニターの「暗い色をより暗く表示してしまう癖」を打ち消すため、あらかじめ暗部を持ち上げる必須処理
    sdrColor = pow(sdrColor, float3(0.4545f, 0.4545f, 0.4545f));

    return float4(sdrColor, 1.0f);
}