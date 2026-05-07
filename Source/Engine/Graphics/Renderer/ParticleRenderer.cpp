#include "ParticleRenderer.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Core/RenderState.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

void ParticleRenderer::Initialize(ID3D11Device *device)
{
    ID3DBlob *blob = nullptr;

    // 1. シェーダーロード
    D3DReadFileToBlob(L"Assets/Shader/particle_vs.cso", &blob);
    if (blob) {
        device->CreateVertexShader(
            blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, vs.GetAddressOf());
        blob->Release();
    }

    D3DReadFileToBlob(L"Assets/Shader/particle_gs.cso", &blob);
    if (blob) {
        device->CreateGeometryShader(
            blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, gs.GetAddressOf());
        blob->Release();
    }

    D3DReadFileToBlob(L"Assets/Shader/particle_ps.cso", &blob);
    if (blob) {
        device->CreatePixelShader(
            blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps.GetAddressOf());
        blob->Release();
    }

    // 2. サンプラー作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
    device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

    // 3. デフォルトテクスチャ作成 (白1x1ピクセル)
    uint32_t             white      = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC texDesc    = {};
    texDesc.Width                   = 1;
    texDesc.Height                  = 1;
    texDesc.MipLevels               = 1;
    texDesc.ArraySize               = 1;
    texDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count        = 1;
    texDesc.Usage                   = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags               = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initData = {&white, sizeof(uint32_t), 0};

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    device->CreateTexture2D(&texDesc, &initData, tex.GetAddressOf());
    device->CreateShaderResourceView(tex.Get(), nullptr, defaultTexture.GetAddressOf());



    // ★ 金型の事前構築
    // 半透明ベースをもらってきて、一部（トポロジーとブレンド）だけをパーティクル用に改造する！
    PipelineStateDesc baseDesc = PipelineStateDesc::DefaultTransparent();
    baseDesc.topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; // 点に変更

    // ① 加算用
    baseDesc.blend = BlendState::Additive;
    _psoAdditive = PipelineState(baseDesc);

    // ② 半透明用
    baseDesc.blend = BlendState::Transparency;
    _psoTransparent = PipelineState(baseDesc);


    _isLoaded = true;
}

void ParticleRenderer::BeginFrame()
{
    _drawCommands.clear(); // 毎フレームバケツを空にする
}

void ParticleRenderer::Draw(const ParticleDrawCommand &cmd)
{
    _drawCommands.push_back(cmd); // バケツに積むだけ
}

void ParticleRenderer::Render(ID3D11DeviceContext *dc)
{
    if (!_isLoaded || _drawCommands.empty()) return;

    RenderState *rs = Graphics::Instance().GetRenderState();

    // 入力アセンブリ設定
    dc->IASetInputLayout(nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    // シェーダー設定
    dc->VSSetShader(vs.Get(), nullptr, 0);
    dc->GSSetShader(gs.Get(), nullptr, 0);
    dc->PSSetShader(ps.Get(), nullptr, 0);

    // サンプラー設定
    ID3D11SamplerState *samplers[] = {sampler.Get()};
    dc->PSSetSamplers(SLOT_SMP_LINEAR, 1, samplers);

    // シーン定数バッファ (カメラ情報) を共通でバインド
    ID3D11Buffer *sceneCB = Graphics::Instance().GetSceneConstantBuffer();
    if (sceneCB) {
        ID3D11Buffer *cbs[] = {sceneCB};
        dc->VSSetConstantBuffers(SLOT_CB_SCENE, 1, cbs);
        dc->GSSetConstantBuffers(SLOT_CB_SCENE, 1, cbs);
        dc->PSSetConstantBuffers(SLOT_CB_SCENE, 1, cbs);
    }

    // =========================================================================
    // ★ 改善点：ステート切り替えを最小化するための「ソート（バッチング）」
    // =========================================================================
    std::sort(_drawCommands.begin(), _drawCommands.end(), [](const ParticleDrawCommand& a, const ParticleDrawCommand& b) {
        // 1. まず「ブレンドモード(加算か半透明か)」で分ける
        if (a.render_mode != b.render_mode) {
            return a.render_mode < b.render_mode;
        }
        // 2. ブレンドモードが同じなら「テクスチャのメモリアドレス（ポインタ）」で分ける
        // ※ポインタ同士の比較は単なる数値比較なので、CPU負荷は実質ゼロです。
        ID3D11ShaderResourceView* texA = ResourceManager::Instance().GetTexture(a.texture);
        ID3D11ShaderResourceView* texB = ResourceManager::Instance().GetTexture(b.texture);
        return texA < texB;
        });

    // 状態を記憶するキャッシュ変数
    int currentRenderMode = -1;
    // キャッシュ用のポインタ（絶対にあり得ないアドレスで初期化しておく）
    ID3D11ShaderResourceView* currentTextureSRV = reinterpret_cast<ID3D11ShaderResourceView*>(-1);

    // 溜まったコマンドを一気に描画
    for (const auto &cmd : _drawCommands) {
        GpuParticleBufferSet *buffers =
            ResourceManager::Instance().GetParticleBuffer(cmd.computeBuffer);
        if (!buffers || !buffers->particleSRV) continue;

        // ★ ステートが変わった時だけ設定を切り替える (GPU負荷激減！)
        if (currentRenderMode != cmd.render_mode) {
            if (cmd.render_mode == 0) {
                _psoAdditive.Apply(dc, rs);
            }
            else {
                _psoTransparent.Apply(dc, rs);
            }
            dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

            currentRenderMode = cmd.render_mode;
        }

        // バッファバインド
        ID3D11ShaderResourceView *srvs[] = {buffers->particleSRV};
        dc->VSSetShaderResources(SLOT_SRV_PARTICLE_BUF, 1, srvs);
        dc->GSSetShaderResources(SLOT_SRV_PARTICLE_BUF, 1, srvs);

        // パーティクル定数バッファバインド
        if (buffers->constantBuffer) {
            ID3D11Buffer *particleCBs[] = {buffers->constantBuffer};
            dc->VSSetConstantBuffers(SLOT_CB_PARTICLE, 1, particleCBs);
            dc->GSSetConstantBuffers(SLOT_CB_PARTICLE, 1, particleCBs);
            dc->PSSetConstantBuffers(SLOT_CB_PARTICLE, 1, particleCBs);
        }

        // ★ テクスチャが変わった時だけバインドし直す
        ID3D11ShaderResourceView* mainTexSRV = ResourceManager::Instance().GetTexture(cmd.texture);
        // テクスチャが無い場合はデフォルトの白テクスチャを割り当てる
        ID3D11ShaderResourceView* actualTexSRV = mainTexSRV ? mainTexSRV : defaultTexture.Get();

        // 記録しておいたポインタのアドレスと違っていれば、DirectXに設定を送信する
        if (currentTextureSRV != actualTexSRV) {
            ID3D11ShaderResourceView* texs[] = { actualTexSRV };
            dc->PSSetShaderResources(SLOT_SRV_MAT_TEX0, 1, texs);

            currentTextureSRV = actualTexSRV; // キャッシュを更新
        }

        // 描画実行
        dc->Draw(cmd.max_particle_count, 0);
    }

    // クリーンアップ
    ID3D11ShaderResourceView *nullSRVs[] = {nullptr};
    dc->VSSetShaderResources(SLOT_SRV_PARTICLE_BUF, 1, nullSRVs);
    dc->GSSetShaderResources(SLOT_SRV_PARTICLE_BUF, 1, nullSRVs);
    dc->PSSetShaderResources(SLOT_SRV_MAT_TEX0, 1, nullSRVs);

    dc->GSSetShader(nullptr, nullptr, 0);
    dc->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(nullptr, 0);
}