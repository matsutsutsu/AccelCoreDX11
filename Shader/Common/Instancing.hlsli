#ifndef INSTANCING_HLSLI
#define INSTANCING_HLSLI

#include "Engine/Graphics/Shader/ShaderResources.h"


// インスタンスデータ構造 (C++側の InstanceData と一致させる)
struct InstanceData
{
    row_major float4x4 World;
    float4 customParams; // x=ディゾルブ進行度などに使う
};

// t11 にバインドされたインスタンスバッファ
StructuredBuffer<InstanceData> g_InstanceData : register(REG_SRV(SLOT_SRV_INSTANCE));

// インスタンスIDからワールド行列を取得する関数
float4x4 GetInstanceWorld(uint instanceID)
{
    return g_InstanceData[instanceID].World;
}

// インスタンスIDからカスタムパラメータを取得する関数
float4 GetInstanceCustomParams(uint instanceID)
{
    return g_InstanceData[instanceID].customParams;
}

#endif // INSTANCING_HLSLI