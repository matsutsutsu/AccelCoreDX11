#pragma once
#include "ECS/System/CCL_System.h"
#include "GPUParticleComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include <d3d11.h>
#include <wrl/client.h>

// =======================================================
// 注文受付窓口 (System)
// 毎フレーム「俺を描いてくれ」というリクエストを収集し、
// Rendererのバケツに引換券を投げ込むだけの軽量クラス
// =======================================================

// GPUパーティクルの描画を担当するシステム
// Priority::Render::MainDraw (900) あたりで実行
class GPUParticleRenderSystem : public CCL::ECS::IfSystem<GPUParticleRenderSystem,
                                    CCL::ECS::Write<GPUParticleComponent>,
                                    CCL::ECS::Read<TransformComponent>> {
  public:
    GPUParticleRenderSystem() : IfSystem("GPUParticleRenderSystem") {}

    ~GPUParticleRenderSystem()
    {
        // デストラクタでリソース解放を呼ぶ
        ReleaseAllResources();
    }

    void Update(float dt) override;

    // 全てのパーティクルコンポーネントのリソースを解放する関数
    void ReleaseAllResources();

  private:
    // 描画用シェーダー
    Microsoft::WRL::ComPtr<ID3D11VertexShader>   vs;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>    ps;

    // ブレンドステート（加算合成用）
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendStateAdd;

    // デプスステencilsステート（Z書き込みOFF、ZテストON）
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStateNoWrite;

    // テクスチャ・サンプラー（パーティクル用）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> defaultTexture;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler;


    bool isLoaded = false;
};