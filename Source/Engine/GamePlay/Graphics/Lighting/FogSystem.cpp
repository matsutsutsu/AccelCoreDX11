#include "FogSystem.h"

// 実装で必要なヘッダ群
#include "ECS/Core/CCL_Chunk.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Shader/ShaderResources.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

FogSystem::FogSystem() : IfSystem("FogSystem") {}


void FogSystem::Update(float dt)
{
    //ID3D11DeviceContext *dc = Graphics::Instance().GetDeviceContext();

    //// 1. デフォルト値（フォグ無効状態）
    //// params.z (enabled) = 0.0f
    //CbFogData data = {
    //    {0.5f, 0.5f, 0.5f, 1.0f}, {0.0f, 1000.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}
    //    // デフォルト中心
    //};
    //ID3D11ShaderResourceView *pNoiseSRV = nullptr;

    //// 2. 世界にあるFogComponentを探す
    //// 最初に見つかった有効なフォグを採用する（あるいはブレンドロジックを入れる）
    //bool found = false;
    //ForEach([&](const FogComponent &fog) {
    //    if (found) return; // すでに見つかっていればスキップ

    //    if (fog.enabled) {
    //        data.color = fog.color;
    //        // 距離 & 高さ
    //        data.params = {fog.start, fog.end, fog.heightStart, fog.heightDensity};
    //        data.center = fog.center;
    //        // ノイズ
    //        data.noiseParams = {
    //            fog.noiseScale, fog.noiseStrength, fog.noiseSpeed.x, fog.noiseSpeed.y};

    //        // リムライト
    //        data.rimColor  = {fog.rimColor.x, fog.rimColor.y, fog.rimColor.z, 1.0f};
    //        data.rimParams = {fog.rimPower, fog.rimStrength, 0.0f, 0.0f};

    //        // テクスチャリソース取得
    //        pNoiseSRV = fog.noiseTextureSRV.Get();

    //        found = true;
    //    }
    //});

    //// 3. 定数バッファを更新 (Map / Unmap)
    //D3D11_MAPPED_SUBRESOURCE mapped;
    //if (SUCCEEDED(dc->Map(_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    //    memcpy(mapped.pData, &data, sizeof(CbFogData));
    //    dc->Unmap(_constantBuffer.Get(), 0);
    //}

    //// 4. バッファをスロットに設定 (SLOT_CB_FOG = 11)
    //// ピクセルシェーダー用
    //dc->PSSetConstantBuffers(SLOT_CB_FOG, 1, _constantBuffer.GetAddressOf());

    // 5. ノイズテクスチャを設定 (Slot 9)
    // テクスチャがある場合はセットし、なければNULLで外す
    // ID3D11ShaderResourceView *srvs[] = {pNoiseSRV};
    // dc->PSSetShaderResources(SLOT_SRV_FOG_NOISE, 1, srvs);

    // ※もし頂点シェーダーでもフォグ距離計算をしているなら以下も必要
    // dc->VSSetConstantBuffers(SLOT_CB_FOG, 1, _constantBuffer.GetAddressOf());
}

// ==========================================
// マクロは必ず .cpp の末尾に1回だけ書く
// ==========================================
//REGISTER_RENDER_SYSTEM(FogSystem, Priority::RenderStage::Environment);