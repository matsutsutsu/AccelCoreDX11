#include "Shader/PostProcess/Bloom.hlsli"

VS_OUT main(uint vertexID : SV_VertexID)
{
    VS_OUT output;
    
    // 三角形1枚で画面を覆う
    // (-1, 1), (3, 1), (-1, -3) の三角形を作ると、UV (0,0)-(2,0)-(0,2) で画面 (0,0)-(1,1) をカバーできる
    float2 grid = float2((vertexID << 1) & 2, vertexID & 2);
    float2 xy = grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    
    output.position = float4(xy, 0.0f, 1.0f);
    output.texcoord = grid;
    
    return output;
}