#ifndef SHAPE_HLSLI
#define SHAPE_HLSLI

// エンジンの共通インクルードを活用する
#include "Shader/Common/Scene.hlsli"      // CbScene (b8) / viewProjection 
#include "Shader/Common/Instancing.hlsli" // g_InstanceData (t11) 

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

#endif