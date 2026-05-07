#pragma once
#include "ECS/System/CCL_System.h"
#include "GPUParticleComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/Graphics/Shader/ShaderReflection.h"

// パーティクルのシミュレーションを行うシステム
// SystemPriority::Render グループで実行推奨 (描画前)
class GPUParticleUpdateSystem : public CCL::ECS::IfSystem<GPUParticleUpdateSystem,
                                    CCL::ECS::Write<GPUParticleComponent>,
                                    CCL::ECS::Read<TransformComponent>> {
  public:
    GPUParticleUpdateSystem() : IfSystem("GPUParticleUpdateSystem") {}

    void InitializeParticle(ID3D11Device *device);
    void Update(float dt) override;

  private:
    // シェーダー
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> initCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> updateCS;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  sampler;

    // ノイズテクスチャのリソース
    Microsoft::WRL::ComPtr<ID3D11Texture3D>          noiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseSRV;

    // ノイズ生成関数
    void CreateNoiseTexture(ID3D11Device *device);

    // ロード済みチェック
    bool isLoaded = false;

    // =======================================================
    // 可読性を上げるためのヘルパー関数群
    // =======================================================

    // 1. バッファの再構築処理
    void RebuildParticleBuffer(GPUParticleComponent &particle);

    // 2. 初期化シェーダー(InitCS)の実行処理
    void DispatchInitCS(ID3D11DeviceContext *dc,
        GPUParticleComponent                &particle,
        const TransformComponent            &transform,
        GpuParticleBufferSet                *buffers);

    // 3. 更新シェーダー(UpdateCS)の実行処理
    void DispatchUpdateCS(ID3D11DeviceContext *dc,
        GPUParticleComponent                  &particle,
        const TransformComponent              &transform,
        float                                  dt,
        GpuParticleBufferSet                  *buffers);
};