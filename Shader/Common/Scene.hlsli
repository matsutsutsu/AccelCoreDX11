#ifndef SCENE_HLSLI
#define SCENE_HLSLI

#include "Engine/Graphics/Shader/ShaderResources.h"

cbuffer CbScene : register(REG_CB(SLOT_CB_SCENE))
{
    row_major float4x4 viewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    row_major float4x4 gView;
    row_major float4x4 gProjection;
    
};

cbuffer CbFog : register(REG_CB(SLOT_CB_FOG)) // register(b11)
{
    float4 gFogColor;
    float4 gFogParams; // x:start, y:end, z:heightStart, w:heightDensity
    float4 gFogCenter; // xyz: center
    float4 gFogNoiseParams; // x:scale, y:strength, z:speedX, w:speedY
    
    float4 gFogRimColor; // rgb: color
    float4 gFogRimParams; // x: power, y: strength
};

// ノイズテクスチャ 
Texture2D gFogNoiseMap : register(REG_SRV(SLOT_SRV_FOG_NOISE));
// サンプラは既存の LinearSampler (s0) を使います

#endif // SCENE_HLSLI