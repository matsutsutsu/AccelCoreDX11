#pragma once
#include "GPUParticleTypes.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
//#include "Engine/Graphics/Core/Graphics.h"
#include <cstring> // strcpy_s, memset用
#include <d3d11.h>
#include "Engine/Graphics/Resource/ResourceManager.h"
#include <unordered_map>
#include <string>

// ComPtr, string, vector (AutoConstantBuffer) を排除して PODライクにする
struct GPUParticleComponent {
    // 設定データ
    ParticleSystemConfig config;

    // 保存用 (デフォルト1万)
    int maxParticles = 1000;

    // 文字列は固定長配列にする (std::string廃止)
    char texturePath[128] = {};
    char colorRampPath[128] = {};

    // 生のポインタ(ComPtrなど)を捨て、安全なチケットにする
    TextureHandle texture;
    TextureHandle colorRamp;

    // 4つの生ポインタを捨て、これ1つに統合！
    ParticleBufferHandle computeBuffer;

    // --- 状態フラグ ---
    bool isInitialized = false; // "false" なら UpdateSystem がリソースを再構築する
    bool needsGpuInit  = true;  // "true" なら InitCS (初期化シェーダー) を走らせる

    // コンストラクタはシンプルに初期化するだけ
    GPUParticleComponent()
    {
        std::memset(texturePath, 0, sizeof(texturePath));
        std::memset(colorRampPath, 0, sizeof(colorRampPath));
        config = ParticleSystemConfig();
    }

    GPUParticleComponent(int count) : GPUParticleComponent()
    {
        maxParticles              = count;
        config.max_particle_count = count;
    }

    void TriggerBurst() { config.burst_trigger = 1.0f; }
};