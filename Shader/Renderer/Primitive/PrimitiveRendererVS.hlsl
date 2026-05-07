#include "Shader/Renderer/Primitive/PrimitiveRenderer.hlsli"

VS_OUT main(VS_IN vin, uint instanceID : SV_InstanceID)
{
    VS_OUT vout;
    
    float4x4 world = GetInstanceWorld(instanceID);
    float4 instanceColor = GetInstanceCustomParams(instanceID);
    
    // =========================================================
    // 1. 暗号（カプセルパラメータ）の抽出
    // =========================================================
    bool isCapsule = (world[0].w > 0.5f);
    
    float radius = 1.0f;
    float cylinderHeight = 1.0f;
    
    if (isCapsule)
    {
        // C++の _24, _34 に隠したパラメータを直接読み取る！
        radius = max(world[1].w, 0.0001f);
        cylinderHeight = max(world[2].w, 0.0f);
    }

    // ★重要：後の座標変換がおかしくならないように、空き部屋を全て 0.0 に掃除する
    world[0].w = 0.0f;
    world[1].w = 0.0f;
    world[2].w = 0.0f;
    
    float3 vPos = vin.position.xyz;
    float3 vNormal = vin.normal.xyz;

    // =========================================================
    // 2. カプセル専用のモーフィング処理
    // =========================================================
    if (isCapsule)
    {
        // ★ C++側で行列をスケールしていない（Identity）ため、
        // ゼロスケールによる「回転情報の破壊（真っ平らバグ）」は発生しません。
        // 面倒なスケールの逆算（length等）も不要になり、処理も高速化しました！

        float halfHeight = cylinderHeight * 0.5f;
        
        if (vPos.y > 0.51f)
        {
            vPos = (vPos - float3(0.0f, 0.5f, 0.0f)) * radius + float3(0.0f, halfHeight, 0.0f);
        }
        else if (vPos.y < -0.51f)
        {
            vPos = (vPos - float3(0.0f, -0.5f, 0.0f)) * radius + float3(0.0f, -halfHeight, 0.0f);
        }
        else
        {
            vPos.x *= radius;
            vPos.z *= radius;
            vPos.y = (vPos.y + 0.5f) * cylinderHeight - halfHeight;
            vNormal.y = 0.0f;
        }
    }

    // =========================================================
    // 3. ワールド座標と法線の適用
    // =========================================================
    float4 worldPos = mul(float4(vPos, 1.0f), world);
    vout.position = mul(worldPos, viewProjection);
    
    // 法線の計算
    float3 wNormal = mul(float4(vNormal, 0.0f), world).xyz;
    float len = length(wNormal);
    if (len < 0.00001f)
    {
        vout.worldNormal = float3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        vout.worldNormal = wNormal / len;
    }
    
    vout.color = instanceColor;

    return vout;
}