// Graphics/Shader/ShaderResources.h

#ifndef SHADER_RESOURCES_H
#define SHADER_RESOURCES_H

// ==================================================================
// 1. 【単一の真実のソース】 レジスタ番号の定義
// ※ ここに1回書くだけで、C++とHLSLの両方に適用されます。
// ==================================================================

// --- Constant Buffers (CB) ---
#define SLOT_CB_MATERIAL        0
#define SLOT_CB_MESH            1

#define SLOT_CB_SKYBOX          2
#define SLOT_CB_BLOOM           0
#define SLOT_CB_TONEMAP         1
#define SLOT_CB_SCENE           8
#define SLOT_CB_LIGHT           9
#define SLOT_CB_SHADOW          10
#define SLOT_CB_FOG             11
#define SLOT_CB_SKELETON        12
#define SLOT_CB_PARTICLE        13

// --- Shader Resources (SRV) ---
#define SLOT_SRV_MAT_TEX0       0
#define SLOT_SRV_MAT_TEX1       1
#define SLOT_SRV_MAT_TEX2       2
#define SLOT_SRV_PARTICLE_BUF   3
#define SLOT_SRV_SCENE_DEPTH    4
#define SLOT_SRV_BLOOM_IN       0
#define SLOT_SRV_BLOOM_HIGHRES  1
#define SLOT_SRV_FOG_NOISE      9
#define SLOT_SRV_SHADOW         10
#define SLOT_SRV_INSTANCE       11
#define SLOT_SRV_BONE           12
#define SLOT_SRV_GLOBAL_NOISE   13

// ==========================================
// IBL用のテクスチャスロット (14, 15, 16)
// ==========================================
#define SLOT_SRV_IBL_IRRADIANCE 14
#define SLOT_SRV_IBL_PREFILTER  15
#define SLOT_SRV_IBL_BRDFLUT    16
// 背景（Skybox）描画専用の超高解像度テクスチャスロット
#define SLOT_SRV_SKYBOX_BG      17

// --- Unordered Access Views (UAV) ---
#define SLOT_UAV_PARTICLE_BUF   0

// --- Samplers (SMP) ---
#define SLOT_SMP_DEFAULT        0
#define SLOT_SMP_LINEAR         1
#define SLOT_SMP_ANISOTROPIC    2
// ==========================================
// IBL専用サンプラスロット (3)
// ==========================================
#define SLOT_SMP_IBL            3
#define SLOT_SMP_SHADOW         10
#define SLOT_SMP_BLOOM          1


// ==================================================================
// 2. 【HLSL専用】 文字結合ギミック (C++コンパイラは無視する)
// ==================================================================
#ifndef __cplusplus
    // マクロ展開の仕様上、数値を "b" などの文字と結合するには
    // この「2段階クッション」が絶対に必要です。
    #define REG_CB_IMPL(slot)  b##slot
    #define REG_SRV_IMPL(slot) t##slot
    #define REG_UAV_IMPL(slot) u##slot
    #define REG_SMP_IMPL(slot) s##slot

    #define REG_CB(slot)  REG_CB_IMPL(slot)
    #define REG_SRV(slot) REG_SRV_IMPL(slot)
    #define REG_UAV(slot) REG_UAV_IMPL(slot)
    #define REG_SMP(slot) REG_SMP_IMPL(slot)
#endif

#endif // SHADER_RESOURCES_H