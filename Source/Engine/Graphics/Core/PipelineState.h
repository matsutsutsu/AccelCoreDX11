#pragma once
#include "Engine/Graphics/Core/RenderState.h"
#include <d3d11.h>

// 1. PSOの「設計図」
struct PipelineStateDesc {
    BlendState               blend = BlendState::Opaque;
    DepthState               depth = DepthState::TestAndWrite;
    RasterizerState          rasterizer = RasterizerState::SolidCullBack;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // =========================================================
    //  テンプレート工場：ここからベースをもらえば1行で済む！
    // =========================================================

    // ① 不透明オブジェクト用のベース（基本中の基本）
    static PipelineStateDesc DefaultOpaque() {
        PipelineStateDesc desc;
        desc.blend = BlendState::Opaque;
        desc.depth = DepthState::TestAndWrite;
        desc.rasterizer = RasterizerState::SolidCullBack;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        return desc;
    }

    // ② 半透明・UI用のベース（深度書き込みなし）
    static PipelineStateDesc DefaultTransparent() {
        PipelineStateDesc desc;
        desc.blend = BlendState::Transparency;
        desc.depth = DepthState::TestOnly; // 奥が消えないようにテストのみ
        desc.rasterizer = RasterizerState::SolidCullNone; // 裏面も描画する
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        return desc;
    }

    // =========================================================
    // ③ トレイル・パーティクル等の加算合成（光るエフェクト）用
    // =========================================================
    static PipelineStateDesc DefaultAdditive() {
        PipelineStateDesc desc;
        // 加算合成のブレンドステート（※RenderState.hに Additive がある前提）
        desc.blend = BlendState::Additive;
        // 半透明なので深度テストはするが、Zバッファへの書き込みはしない
        desc.depth = DepthState::TestOnly;
        // 剣の軌跡は裏からも見える必要があるのでカリングなし
        desc.rasterizer = RasterizerState::SolidCullNone;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        return desc;
    }
};

// 2. 完成した「金型（PSO）」本体
class PipelineState {
public:
    PipelineState() = default;

    // 設計図を受け取って金型を固める
    PipelineState(const PipelineStateDesc& desc) : _desc(desc) {}

    // 描画の直前に、GPUに対して「この金型通りにしろ！」と強制上書きする
    void Apply(ID3D11DeviceContext* dc, const RenderState* rs) const {
        dc->IASetPrimitiveTopology(_desc.topology);
        dc->OMSetBlendState(rs->GetBlendState(_desc.blend), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(_desc.depth), 0);
        dc->RSSetState(rs->GetRasterizerState(_desc.rasterizer));
    }

private:
    PipelineStateDesc _desc;
};