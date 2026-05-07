#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <mutex>
#include <unordered_map>

#include "Engine/Core/Math/StringHash.h"
#include "ResourcePool.h"

// --- 依存するリソースの先行宣言 & インクルード ---
#include "ModelResource.h"
#include "Model.h"
#include "MaterialData.h"
#include "Engine/Graphics/Core/RenderTarget.h"
#include "Engine/GamePlay/Animation/Data/AnimSequence.h"
#include "Engine/GamePlay/Animation/Data/AnimationCurve.h"

// ============================================================================
// [Data Structures]
// ============================================================================

// コンピュートシェーダー用のバッファ群
struct GpuParticleBufferSet {
    ID3D11Buffer* particleBuffer = nullptr;
    ID3D11UnorderedAccessView* particleUAV    = nullptr;
    ID3D11ShaderResourceView* particleSRV    = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;

    GpuParticleBufferSet() = default;
    GpuParticleBufferSet(const GpuParticleBufferSet&) = delete;
    GpuParticleBufferSet& operator=(const GpuParticleBufferSet&) = delete;

    GpuParticleBufferSet(GpuParticleBufferSet&& other) noexcept;
    GpuParticleBufferSet& operator=(GpuParticleBufferSet&& other) noexcept;
    ~GpuParticleBufferSet();
};

// ============================================================================
// [Handle Definitions]
// ============================================================================
using TextureResource      = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;
using TextureHandle        = CCL::Handle<TextureResource>;
using ModelHandle          = CCL::Handle<ModelResource>;
using ModelInstanceHandle  = CCL::Handle<Model>;
using MaterialHandle       = CCL::Handle<MaterialData>;
using ParticleBufferHandle = CCL::Handle<GpuParticleBufferSet>;
using RenderTargetHandle   = CCL::Handle<RenderTarget>;
using AnimSequenceHandle   = CCL::Handle<AnimSequence>;


// ============================================================================
// [ResourceManager]
// ============================================================================
class ResourceManager {
private:
    ResourceManager() = default;
    ~ResourceManager() = default;

public:
    static ResourceManager& Instance() {
        static ResourceManager instance;
        return instance;
    }

    // ---------------------------------------------------------
    // [1] Texture (Static)
    // ---------------------------------------------------------
    TextureHandle LoadTexture(const char* filename);
    TextureHandle RegisterTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);
    ID3D11ShaderResourceView* GetTexture(TextureHandle handle);
    void UnloadTexture(TextureHandle handle);

    // ---------------------------------------------------------
    // [2] Model Resource (Static Data)
    // ---------------------------------------------------------
    ModelHandle LoadModelResource(const char* filename);
    ModelHandle AllocateEmptyModelResource();
    ModelResource* GetModel(ModelHandle handle);
    void UnloadModel(ModelHandle handle);

    // ハッシュからパス文字列を逆引きする機能（GUI/シリアライズ用）
    std::string GetAssetPath(uint32_t hash);

    // ---------------------------------------------------------
    // [3] Model Instance (Dynamic Data)
    // ---------------------------------------------------------
    ModelInstanceHandle CreateModelInstance(const char* filename);
    ModelInstanceHandle CreateModelInstanceFromHash(uint32_t hash);
    Model* GetModelInstance(ModelInstanceHandle handle);
    void UnloadModelInstance(ModelInstanceHandle handle);

    // ---------------------------------------------------------
    // [4] Material
    // ---------------------------------------------------------
    MaterialHandle CreateMaterial();
    MaterialHandle CloneMaterial(MaterialHandle source);
    MaterialData* GetMaterial(MaterialHandle handle);
    void UnloadMaterial(MaterialHandle handle);

    // ---------------------------------------------------------
    // [5] Animation Sequence
    // ---------------------------------------------------------
    const AnimSequence* LoadAnimSequence(const char* filename);
    const AnimationCurve* LoadAnimCurve(const char* filepath);

    // ---------------------------------------------------------
    // [6] Render Target (GPU Resource)
    // ---------------------------------------------------------
    RenderTargetHandle CreateRenderTarget(int width, int height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    RenderTarget* GetRenderTarget(RenderTargetHandle handle);
    void UnloadRenderTarget(RenderTargetHandle handle);

    // ---------------------------------------------------------
    // [7] GPU Particle Buffer
    // ---------------------------------------------------------
    ParticleBufferHandle CreateParticleBuffer();
    GpuParticleBufferSet* GetParticleBuffer(ParticleBufferHandle handle);
    void UnloadParticleBuffer(ParticleBufferHandle handle);

    // ---------------------------------------------------------
    // [Debug]
    // ---------------------------------------------------------
    void DrawDebugGUI();

private:
    // --- Resource Pools (連続メモリ配置) ---
    CCL::ResourcePool<TextureResource>      m_texturePool;
    CCL::ResourcePool<ModelResource>        m_modelPool;
    CCL::ResourcePool<Model>                m_modelInstancePool;
    CCL::ResourcePool<MaterialData>         m_materialPool;
    CCL::ResourcePool<AnimSequence>         m_animSequencePool;
    CCL::ResourcePool<RenderTarget>         m_renderTargetPool;
    CCL::ResourcePool<GpuParticleBufferSet> m_particleBufferPool;
    CCL::ResourcePool<AnimationCurve>       m_animCurvePool; 

    // --- Registries (重複ロード防止のキャッシュ) ---
    std::unordered_map<uint32_t, TextureHandle>      m_textureRegistry;
    std::unordered_map<uint32_t, ModelHandle>        m_modelRegistry;
    std::unordered_map<uint32_t, AnimSequenceHandle> m_animSequenceRegistry;
    std::unordered_map<uint32_t, std::string>        m_assetPathRegistry;
    std::unordered_map<uint32_t, CCL::Handle<AnimationCurve>> m_animCurveRegistry; 

    // スレッドセーフ用ミューテックス
    std::mutex _mutex;

#ifdef _DEBUG
    std::unordered_map<uint32_t, std::string> m_debugNameRegistry;
#endif
};