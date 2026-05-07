#include "GPUParticleRenderSystem.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Renderer/ParticleRenderer.h"
#include "Engine/Graphics/Resource/ResourceManager.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void GPUParticleRenderSystem::Update(float dt)
{
        // シングルトンではなくWorldから取得する
    if (!_world->HasResource<ParticleRenderer *>()) return;
    auto *renderer = _world->GetResource<ParticleRenderer *>();

    ForEach([&](GPUParticleComponent &particle, const TransformComponent &transform) {
        // --- 1. 描画をスキップする条件のチェック ---

        // プール待機中（親ID=0）のものは描画しない
        if (transform.parentID == 0) return;


        // リソースが未初期化、またはバッファが存在しない場合はスキップ
        if (!particle.isInitialized || !particle.computeBuffer.IsValid()) return;

        // --- 2. 引換券（コマンド）の作成 ---
        ParticleDrawCommand cmd;
        cmd.computeBuffer      = particle.computeBuffer;
        cmd.texture            = particle.texture;
        cmd.max_particle_count = particle.config.max_particle_count;
        cmd.render_mode        = particle.config.render_mode;

        // --- 3. バケツに積む（絶対にここでDrawしない！） ---
        renderer->Draw(cmd);
    });
}

void GPUParticleRenderSystem::ReleaseAllResources()
{
    // 全てのコンポーネントが持っているバッファの部屋を破壊する
    ForEach([](GPUParticleComponent &particle, const TransformComponent &transform) {
        if (particle.computeBuffer.IsValid()) {
            ResourceManager::Instance().UnloadParticleBuffer(particle.computeBuffer);
            particle.computeBuffer = ParticleBufferHandle{}; // 空にする
            particle.isInitialized = false;
        }
    });
}

// これは手動で管理するのでマクロはやめる
REGISTER_RENDER_SYSTEM(GPUParticleRenderSystem, Priority::RenderStage::R08_Main);