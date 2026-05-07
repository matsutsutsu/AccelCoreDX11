#include "Shader/Common/Scene.hlsli" // cameraPosition など

// C++から受け取る逆行列用の定数バッファ
cbuffer CbSkybox : register(REG_CB(SLOT_CB_SKYBOX))
{
    row_major float4x4 invViewProj;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float3 rayDir : TEXCOORD;
};

VS_OUT main(uint vertexID : SV_VertexID)
{
    VS_OUT vout;
    
    // 巨大な三角形を1枚生成 (頂点バッファを使わずに全画面を覆う魔法の計算)
    float2 texcoord = float2((vertexID << 1) & 2, vertexID & 2);
    float2 clipPos = texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    
    // Zを 1.0 (最奥) に固定。Wも 1.0 にして透視投影除算後も 1.0 を維持する
    vout.pos = float4(clipPos, 1.0f, 1.0f);
    
    // スクリーン座標からワールド空間の視線方向（Ray）を逆算
    float4 worldPos = mul(float4(clipPos, 1.0f, 1.0f), invViewProj);
    vout.rayDir = (worldPos.xyz / worldPos.w) - cameraPosition.xyz;
    
    return vout;
}