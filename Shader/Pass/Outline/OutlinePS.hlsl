#include "Engine/Graphics/Shader/ShaderResources.h"

cbuffer CbOutline : register(b1)
{
    float4 gOutlineColor;
    float gOutlineSize;
    float3 gOutlinePad;
};

float4 main() : SV_TARGET
{
    // 指定された色（基本は黒）を返すだけ
    return gOutlineColor;
}