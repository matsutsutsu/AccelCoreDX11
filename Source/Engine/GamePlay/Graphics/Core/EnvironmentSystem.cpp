#include "EnvironmentSystem.h"

// 必要なインクルード群
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Resource/ResourceManager.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Core/Light.h"
#include "Engine/Graphics/Core/Camera.h"

// 画面サイズ取得のため
#include "Engine/Graphics/Core/Graphics.h"

// 環境を構成するコンポーネント
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "Engine/GamePlay/Graphics/Lighting/FogComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX;

EnvironmentSystem::EnvironmentSystem() : SystemBase("EnvironmentSystem")
{
    // 1. ディゾルブ用グローバルノイズテクスチャのロード
    TextureHandle noiseTexHandle =
        ResourceManager::Instance().LoadTexture("Assets/Textures/Shader/Noise.png");
        
    // ハンドル（チケット）が有効か確認
    if (noiseTexHandle.IsValid()) {
        // マネージャーにチケットを渡して、実体（SRVのポインタ）をもらう
        _globalNoiseSRV = ResourceManager::Instance().GetTexture(noiseTexHandle);
    }

    // =========================================================
    // IBLテクスチャのロード
    // ※パスはあなたのプロジェクトの構成に合わせて変更してください
    // =========================================================
    auto irrHandle = ResourceManager::Instance().LoadTexture("Assets/Textures/IBL/ibl_sky_iem.dds");
    auto preHandle = ResourceManager::Instance().LoadTexture("Assets/Textures/IBL/ibl_sky_pmrem.dds");
    auto lutHandle = ResourceManager::Instance().LoadTexture("Assets/Textures/IBL/ibl_brdf_lut.png");

    if (irrHandle.IsValid()) _irradianceMapSRV = ResourceManager::Instance().GetTexture(irrHandle);
    if (preHandle.IsValid()) _prefilterMapSRV = ResourceManager::Instance().GetTexture(preHandle);
    if (lutHandle.IsValid()) _brdfLutSRV = ResourceManager::Instance().GetTexture(lutHandle);


    // 2. フォグ用定数バッファの作成
    GpuResourceUtils::CreateDynamicConstantBuffer(
        Graphics::Instance().GetDevice(), sizeof(CbFogData), _fogConstantBuffer.GetAddressOf());
}

std::vector<CCL::ECS::TypeID> EnvironmentSystem::GetReadTypes() const
{
    return {CCL::ECS::TypeInfo<VirtualCamera>::ID(),
        CCL::ECS::TypeInfo<FogComponent>::ID(),
        CCL::ECS::TypeInfo<TransformComponent>::ID()};
}

void EnvironmentSystem::Update(float dt)
{
    if (!_world || !_world->HasResource<ModelRenderer *>()) return;
    ModelRenderer *renderer = _world->GetResource<ModelRenderer *>();
    if (!renderer) return;

    // -------------------------------------------------------------
    // 1. カメラ情報の収集と定数バッファ(CbScene)へのセットアップ
    // -------------------------------------------------------------
    // VirtualCameraコンポーネントを直接見るのではなく、
    // CameraBrainSystemが滑らかにブレンドしてくれた「Cameraリソース」を使います。
    if (_world->HasResource<Camera *>()) {
        Camera *mainCamera = _world->GetResource<Camera *>();
        if (mainCamera) {
            // ブレンド済みの滑らかな行列を取得
            DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&mainCamera->GetView());
            DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&mainCamera->GetProjection());

            DirectX::XMFLOAT4X4 viewMat, projMat, viewProjMat;
            DirectX::XMStoreFloat4x4(&viewMat, V);
            DirectX::XMStoreFloat4x4(&projMat, P);
            DirectX::XMStoreFloat4x4(&viewProjMat, V * P);

            // リフレクションベースの定数バッファ更新（★これらを削除して書き換え）
            _sceneData.viewProjection = viewProjMat;
            _sceneData.view = viewMat;
            _sceneData.projection = projMat;

            // カメラのワールド座標
            DirectX::XMFLOAT3 eye = mainCamera->GetEye();
            DirectX::XMFLOAT4 eyePos = { eye.x, eye.y, eye.z, 1.0f };
            _sceneData.cameraPosition = eyePos; // ★書き換え

            // フラスタムの更新もこの滑らかな行列で行う！
            //renderer->UpdateFrustum(V, P);
        }
    }

    // -------------------------------------------------------------
    // 2. ライト情報の収集 (CbScene用)
    // -------------------------------------------------------------
    if (_world->HasResource<LightManager *>()) {
        auto *lightManager = _world->GetResource<LightManager *>();
        if (lightManager) {
            const auto &directionalLight = lightManager->GetDirectionalLight();

            // ライト情報の収集 (CbScene用)
            XMFLOAT4 lightDir = { directionalLight.direction.x,
                directionalLight.direction.y,
                directionalLight.direction.z,
                0.0f };
            _sceneData.lightDirection = lightDir;

            XMFLOAT4 lightCol = {
                directionalLight.color.x, directionalLight.color.y, directionalLight.color.z, 1.0f };
            _sceneData.lightColor = lightCol;
        }
    }

}

void EnvironmentSystem::BindGlobalResources(ID3D11DeviceContext *dc)
{
    if (!_world) return;

    // -------------------------------------------------------------
    // 1. 定数バッファのGPU転送とバインド
    // -------------------------------------------------------------
    // キャッシュしたシーンデータをGPUのバッファに転送（Map/Unmapより軽いUpdateSubresourceを使用）
    ID3D11Buffer* sceneBuf = Graphics::Instance().GetSceneConstantBuffer();
    if (sceneBuf) {
        dc->UpdateSubresource(sceneBuf, 0, nullptr, &_sceneData, 0, 0);
    }

    if (_world->HasResource<ModelRenderer *>()) {
        if (auto *renderer = _world->GetResource<ModelRenderer *>()) {
            // CbScene 等の最新データをGPUに流し込む
            renderer->BindAllGlobalConstantBuffers(dc);
        }
    }

    // -------------------------------------------------------------
    // 2. グローバルノイズテクスチャのバインド (t3)
    // -------------------------------------------------------------
    if (_globalNoiseSRV) {
        ID3D11ShaderResourceView *srvs[] = {_globalNoiseSRV.Get()};
        dc->PSSetShaderResources(SLOT_SRV_GLOBAL_NOISE, 1, srvs);
    }

    // -------------------------------------------------------------
    // 3. フォグ情報の定数バッファ更新とバインド
    // -------------------------------------------------------------
    // デフォルト値（フォグコンポーネントが無い場合）
    CbFogData fogData = {{0.5f, 0.5f, 0.5f, 1.0f},
        {0.0f, 1000.0f, 10.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f}};

    ID3D11ShaderResourceView *fogNoiseSrv = nullptr;

    auto fogView = _world->View<FogComponent>();
    if (!fogView.empty()) {
        auto *fog = _world->GetComponent<FogComponent>(fogView[0]);
        if (fog) {
            // ECSのコンポーネントからGPU転送用の構造体に詰め替える
            fogData.color  = fog->color;
            fogData.params = {fog->start, fog->end, fog->heightStart, fog->heightDensity};
            fogData.center = fog->center; // 拡張用

            fogData.noiseParams = {
                fog->noiseScale, fog->noiseStrength, fog->noiseSpeed.x, fog->noiseSpeed.y};

            fogData.rimColor  = {fog->rimColor.x, fog->rimColor.y, fog->rimColor.z, 1.0f};
            fogData.rimParams = {fog->rimPower, fog->rimStrength, 0.0f, 0.0f};

            fogNoiseSrv = fog->noiseTextureSRV.Get();
        }
    }

    // GPUへ転送 (Map / Unmap)
    if (_fogConstantBuffer) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(dc->Map(_fogConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &fogData, sizeof(CbFogData));
            dc->Unmap(_fogConstantBuffer.Get(), 0);
        }

        // バッファをスロットに設定 (SLOT_CB_FOG = 11)
        dc->PSSetConstantBuffers(SLOT_CB_FOG, 1, _fogConstantBuffer.GetAddressOf());
    }

    // ノイズテクスチャをスロットに設定 (SLOT_SRV_FOG_NOISE = 9)
    dc->PSSetShaderResources(SLOT_SRV_FOG_NOISE, 1, &fogNoiseSrv);

    // =========================================================
    // IBLリソースのグローバルバインド (スロット t12, t13, t14)
    // =========================================================
    ID3D11ShaderResourceView* iblSRVs[3] = { nullptr, nullptr, nullptr };
    if (_irradianceMapSRV) iblSRVs[0] = _irradianceMapSRV.Get();
    if (_prefilterMapSRV)  iblSRVs[1] = _prefilterMapSRV.Get();
    if (_brdfLutSRV)       iblSRVs[2] = _brdfLutSRV.Get();

    dc->PSSetShaderResources(SLOT_SRV_IBL_IRRADIANCE, 3, iblSRVs);

    // IBL用のサンプラ（クランプ）もここで一緒にスロット12(s12)にバインドしておく
    ID3D11SamplerState* iblSampler = Graphics::Instance().GetRenderState()->GetSamplerState(SamplerState::LinearClamp);
    dc->PSSetSamplers(SLOT_SMP_IBL, 1, &iblSampler);

}

REGISTER_RENDER_SYSTEM(EnvironmentSystem, Priority::RenderStage::R02_Environment);