//#include "Shader/Common/Common.hlsli"
//#include "Shader/Common/Scene.hlsli"
//#include "BoidsData.hlsli"

//StructuredBuffer<BoidData> BoidsBuffer : register(t11); // SRVとして読み込む

//struct VS_IN
//{
//    float4 pos : POSITION;
//    float3 normal : NORMAL;
//    float2 uv : TEXCOORD0;
//    uint instanceId : SV_InstanceID; // 何番目のBoidか
//};

//struct VS_OUT
//{
//    float4 pos : SV_POSITION;
//    float3 normal : NORMAL;
//    float2 uv : TEXCOORD0;
//};

//VS_OUT main(VS_IN vin)
//{
//    VS_OUT vout;
    
//    // インスタンスIDから自分のBoidデータを取得
//    BoidData boid = BoidsBuffer[vin.instanceId];

//    // 進行方向に向けるための回転行列（簡易版）
//    float3 forward = normalize(boid.velocity);
//    float3 up = float3(0, 1, 0);
//    float3 right = normalize(cross(up, forward));
//    up = cross(forward, right);

//    float4x4 worldMatrix = float4x4(
//        right.x * boid.scale, right.y * boid.scale, right.z * boid.scale, 0,
//        up.x * boid.scale, up.y * boid.scale, up.z * boid.scale, 0,
//        forward.x * boid.scale, forward.y * boid.scale, forward.z * boid.scale, 0,
//        boid.position.x, boid.position.y, boid.position.z, 1
//    );

//    float4 worldPos = mul(vin.pos, worldMatrix);
//    vout.pos = mul(worldPos, ViewProjection);
//    vout.normal = mul(vin.normal, (float3x3) worldMatrix);
//    vout.uv = vin.uv;

//    return vout;
//}