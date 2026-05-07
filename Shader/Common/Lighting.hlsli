#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

// --- 1. 構造体定義 ---
struct LightResult
{
    float3 diffuse;
    float3 specular;
};

struct SurfaceInfo
{
    float3 pos; // ワールド座標
    float3 normal; // 法線（法線マップ適用済み）
    float3 viewDir; // 視点への方向
    float3 color; // テクスチャ×マテリアル色
    float  alpha; // 透明度
};

// --- 2. ライト用定数バッファ ---
struct DirectionalLight
{
    float3 direction;
    float intensity;
    float3 color;
    float padding;
    // 半球ライティング用
    float4 gSkyColor; // rgb:色, a:強度
    float4 gGroundColor; // rgb:色, a:強度
};
struct PointLight
{
    float3 position;
    float range;
    float3 color;
    float intensity;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
    float active;
};
struct SpotLight
{
    float3 position;
    float range;
    float3 direction;
    float innerCos;
    float3 color;
    float outerCos;
    float intensity;
    float active;
    float padding1;
    float padding2;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
    float padding;
};

// C++側の LightConstantBuffer と完全一致させる
cbuffer CbLight : register(REG_CB(SLOT_CB_LIGHT))
{
    DirectionalLight gDirLight;   
    PointLight gPointLights[8]; // MAX_POINT_LIGHTS と合わせる
    SpotLight gSpotLights[8]; // MAX_SPOT_LIGHTS と合わせる
    int gPointLightCount;
    int gSpotLightCount;
    float2 gPadding;
};

// --- 3. 反射計算関数 ---

//--------------------------------------------
//	ランバート拡散反射計算関数
//--------------------------------------------
// N:法線(正規化済み)
// L:入射ベクトル(正規化済み)
// C:入射光(色・強さ)
// K:反射率
float3 CalcLambert(float3 N, float3 L, float3 C, float3 K)
{
    return C * saturate(dot(N, -L)) * K;
}

// 従来の CalcLambert などをそのまま残しつつ、
// 「生の光の当たり具合（0～1）」を返す関数を用意する
float CalcLightIntensity(float3 N, float3 L)
{
    return saturate(dot(N, -L));
}

//--------------------------------------------
//	フォンの鏡面反射計算関数
//--------------------------------------------
// N:法線(正規化済み)
// L:入射ベクトル(正規化済み)
// E:視線ベクトル(正規化済み)
// C:入射光(色・強さ)
// K:反射率
float3 CalcPhongSpecular(float3 N, float3 L, float3 E, float3 C, float3 K)
{
    float3 R = reflect(L, N);
    return C * pow(max(dot(E, R), 0), 128) * K;
}

//--------------------------------------------
// 半球ライティング
//--------------------------------------------
// normal:法線(正規化済み)
// up:上方向（片方）
// sky_color:空(上)色
// ground_color:地面(下)色
// hemisphere_weight:重み
float3 CalcHemiSphereLight(float3 normal, float3 up, float3 sky_color, float3 ground_color, float4 hemisphere_weight)
{
    float factor = dot(normal, up) * 0.5f + 0.5f;
    return lerp(ground_color, sky_color, factor) * hemisphere_weight.x;
}

//--------------------------------------------
// 法線マップから最終的な法線を計算する関数
//--------------------------------------------
float3 CalculateWorldNormal(
    float3 worldNormal,
    float4 worldTangent,
    float2 texcoord,
    Texture2D normalMap,
    SamplerState smp)
{
    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    float3 B = normalize(cross(N, T) * worldTangent.w);
    
    float3 normalSample = normalMap.Sample(smp, texcoord).xyz * 2.0f - 1.0f;
    
    // TBN行列による変換
    return normalize((normalSample.x * T) + (normalSample.y * B) + (normalSample.z * N));
}



// --- 4. 統合ライティング関数 ---
LightResult CalculateAllLights(SurfaceInfo surf, float shadowWeight)
{
    LightResult res;
    res.diffuse = 0;
    res.specular = 0;

    // --- 1. 平行光源 (Directional Light) ---
    {
        float3 L = normalize(gDirLight.direction);
        float3 LC = gDirLight.color * gDirLight.intensity;
        res.diffuse += CalcLambert(surf.normal, L, LC, 1.0f) * shadowWeight;
        res.specular += CalcPhongSpecular(surf.normal, L, surf.viewDir, LC, 1.0f) * shadowWeight;
    }
    
    // --- 2. 点光源 (Point Light) ---
    for (int i = 0; i < gPointLightCount; ++i)
    {
        if (gPointLights[i].active < 0.5f)
            continue;
        float3 L = surf.pos - gPointLights[i].position;
        float d = length(L);
        L /= d;
        if (d < gPointLights[i].range)
        {
            float atten = 1.0f / (gPointLights[i].constantAttenuation + gPointLights[i].linearAttenuation * d + gPointLights[i].quadraticAttenuation * (d * d));
            float3 LC = gPointLights[i].color * gPointLights[i].intensity * atten * saturate(1.0f - d / gPointLights[i].range);
            res.diffuse += CalcLambert(surf.normal, L, LC, 1.0f);
            res.specular += CalcPhongSpecular(surf.normal, L, surf.viewDir, LC, 1.0f);
        }
    }

    // --- 3. スポットライト (Spot Light) ---
    for (int j = 0; j < gSpotLightCount; ++j)
    {
           // 1. 有効フラグのチェック（0.5未満なら無効とみなす）
        if (gSpotLights[j].active < 0.5f)
            continue;
        
        // 2. ライトへの方向ベクトルと距離の算出
        float3 L = surf.pos - gSpotLights[j].position;
        float d = length(L);
        L /= d; // 正規化

        // 3. 影響範囲内か判定
        if (d < gSpotLights[j].range)
        {
            // --- 角度による減衰計算 ---
            float cosAngle = dot(normalize(gSpotLights[j].direction), L);
            // 内角(inner)と外角(outer)の間で滑らかに減衰させる
            float angleAtten = saturate((cosAngle - gSpotLights[j].outerCos) / (gSpotLights[j].innerCos - gSpotLights[j].outerCos));
            
            // --- 距離による減衰計算 (3係数モデル) ---
            // constant(定数), linear(線形), quadratic(二次) を使用
            float distAtten = 1.0f / (gSpotLights[j].constantAttenuation +
                                      gSpotLights[j].linearAttenuation * d +
                                      gSpotLights[j].quadraticAttenuation * (d * d));
            
            // --- 最終的な光の強さと色の決定 ---
            // intensity(強度)をここで適用
            // 範囲の端でパッと消えないようにするためのフェード
            float3 LC = gSpotLights[j].color * gSpotLights[j].intensity * angleAtten * distAtten * saturate(1.0f - d / gSpotLights[j].range);
            
            // 4. 反射計算に適用
            res.diffuse += CalcLambert(surf.normal, L, LC, 1.0f);
            res.specular += CalcPhongSpecular(surf.normal, L, surf.viewDir, LC, 1.0f);
        }
    }
    return res;
}


#endif // LIGHTING_HLSLI