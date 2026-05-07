#pragma once
#include "Engine/GamePlay/Graphics/Particle/GPUParticleComponent.h"
#include "Engine/Graphics/Resource/ResourceManager.h"
#include <d3d11.h>
#include <vector>
#include <wrl/client.h>
#include "Engine/Graphics/Core/PipelineState.h"

// 描画に必要な情報の「引換券（コマンド）」
struct ParticleDrawCommand {
    ParticleBufferHandle computeBuffer;
    TextureHandle        texture;
    int                  max_particle_count;
    int                  render_mode; // 0: 加算, 1: 半透明
};

// パーティクルの描画を専任で行うクラス
class ParticleRenderer {
  public:
    ParticleRenderer()  = default;
    ~ParticleRenderer() = default;

    void Initialize(ID3D11Device *device);

    // 毎フレームの頭でバケツを空にする
    void BeginFrame();

    // システムから呼ばれ、バケツにコマンドを積むだけ（ここではDrawしない！）
    void Draw(const ParticleDrawCommand &cmd);

    // シーンの最後に一気に描画する
    void Render(ID3D11DeviceContext *dc);

  private:
    std::vector<ParticleDrawCommand> _drawCommands;

    // --- 描画用リソース ---
    Microsoft::WRL::ComPtr<ID3D11VertexShader>   vs;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>    ps;

    Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> defaultTexture;

    bool _isLoaded = false;

    PipelineState _psoAdditive;    // 加算パーティクル用
    PipelineState _psoTransparent; // 半透明パーティクル用
};