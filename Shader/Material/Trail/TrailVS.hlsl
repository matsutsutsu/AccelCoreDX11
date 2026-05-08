// Shader/Material/Trail/TrailVS.hlsl
#include "Shader/Common/Common.hlsli"

// トレイル専用の入力構造体
struct TRAIL_VS_IN
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

// トレイル専用の出力構造体
struct TRAIL_VS_OUT
{
    float4 vertex : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

TRAIL_VS_OUT main(TRAIL_VS_IN vin)
{
    TRAIL_VS_OUT vout;
    
    // CPU側(TrailSystem)ですでにワールド座標に変換されているため、
    // ここではカメラのViewProjection行列を掛けるだけでOKです。
    vout.vertex = mul(float4(vin.position, 1.0f), viewProjection);
    vout.color = vin.color;
    vout.uv = vin.uv;

    return vout;
}