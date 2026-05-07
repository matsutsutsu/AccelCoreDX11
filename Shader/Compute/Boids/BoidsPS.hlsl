//struct VS_OUT
//{
//    float4 pos : SV_POSITION;
//    float3 normal : NORMAL;
//    float2 uv : TEXCOORD0;
//};

//Texture2D mainTex : register(t0);
//SamplerState smp : register(s0);

//float4 main(VS_OUT pin) : SV_TARGET
//{
//    float4 color = mainTex.Sample(smp, pin.uv);
//    // 簡易的なライティング（上からの光）
//    float light = dot(normalize(pin.normal), float3(0, 1, -1)) * 0.5 + 0.5;
//    return float4(color.rgb * light, color.a);
//}