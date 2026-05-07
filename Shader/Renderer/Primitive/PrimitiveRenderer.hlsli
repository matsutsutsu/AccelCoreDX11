#ifndef PRIMITIVE_HLSLI
#define PRIMITIVE_HLSLI

// エンジンの共通インクルードを活用する
#include "Shader/Common/Scene.hlsli"      // CbScene (b8) が定義されている
#include "Shader/Common/Instancing.hlsli" // g_InstanceData (t11) が定義されている

// C++側の D3D11_INPUT_ELEMENT_DESC と完全に一致させる
struct VS_IN
{
    float4 position : POSITION; // 12byte
    float3 normal : NORMAL; // 12byte
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR; // ピクセルシェーダーに渡す色
    float3 worldNormal : NORMAL; // ワールド空間での法線ベクトル
};

#endif