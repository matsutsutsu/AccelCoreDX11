#include "Shader/Common/Common.hlsli"

cbuffer CbMesh : register(REG_CB(SLOT_CB_MESH))
{
	float4		materialColor;
};

Texture2D DiffuseMap : register(REG_SRV(SLOT_SRV_MAT_TEX0));
SamplerState LinearSampler : register(REG_SMP(SLOT_SMP_LINEAR));

float4 main(VS_OUT pin) : SV_TARGET
{
	return DiffuseMap.Sample(LinearSampler, pin.texcoord) * materialColor;
}
