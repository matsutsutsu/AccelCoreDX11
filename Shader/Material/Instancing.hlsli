#ifndef INSTANCING_HLSLI
#define INSTANCING_HLSLI

#include "RegisterDef.hlsli"

// インスタンスデータ構造 (C++側の InstanceData と一致させる)
struct InstanceData
{
    row_major float4x4 World;
};

// t11 にバインドされたインスタンスバッファ
StructuredBuffer<InstanceData> g_InstanceData : register(REG_SRV_INSTANCE);

// インスタンスIDからワールド行列を取得する関数
float4x4 GetInstanceWorld(uint instanceID)
{
    return g_InstanceData[instanceID].World;
}

#endif // INSTANCING_HLSLI