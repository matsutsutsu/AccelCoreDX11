#include "Shader/Compute/GPUParticle/particle.hlsli"
// 共通定義ファイルをインクルード。
// ここには「構造体（VS_OUTなど）」や「定数バッファ（cbuffer）」などの共通情報が定義されている。
// → CPUとGPUでデータ構造を共有するため。

// 定義のみ（VSではVertexIDを使うのでアクセスはしないが、スロットを予約しておく）
StructuredBuffer<particle> particle_buffer : register(REG_SRV(SLOT_SRV_PARTICLE_BUF));

//============================================
// 頂点シェーダ（Vertex Shader）
//============================================
// 役割：ジオメトリシェーダへ「頂点ID情報」だけを渡す。
// ※ 通常の頂点シェーダのように頂点座標を計算して返すわけではない。
//    理由：頂点バッファを使わず、Compute ShaderやGPUバッファ上の
//          パーティクル情報（StructuredBuffer）を直接扱う構成だから。
VS_OUT main(uint vertex_id : SV_VERTEXID)
{
    VS_OUT vout;

    // 入力された頂点ID（0, 1, 2, ...）をそのまま出力構造体に格納
    vout.vertex_id = vertex_id;

    // これをジオメトリシェーダに渡す
    return vout;
}
