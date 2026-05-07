#include "Shader/Renderer/Shape/ShapeRenderer.hlsli"

VS_OUT main(float4 position : POSITION, uint instanceID : SV_InstanceID)
{
    VS_OUT vout;
    
    // インスタンスバッファから、この線の「ワールド行列」と「色」を取り出す
    float4x4 world = GetInstanceWorld(instanceID);
    float4 instanceColor = GetInstanceCustomParams(instanceID);

    // ワールド行列でローカルな線分をスケール＆回転し、配置する
    float4 worldPos = mul(position, world);
    
    // Scene.hlsli の viewProjection を用いてクリップ空間へ
    vout.position = mul(worldPos, viewProjection);
    vout.color = instanceColor;

    return vout;
}