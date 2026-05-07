#pragma once
#include "Editor/Core/EditorWindowManager.h"
#include "Engine/GamePlay/Graphics/Particle/GPUParticleRenderSystem.h"
#include "Game/Scenes/BaseScene.h"

class ParticleScene : public BaseScene {
    std::unique_ptr<EditorWindowManager>     _windowManager;

    // エディタの全記憶を保持するコンテキスト（毎フレーム破棄されない）
    EditorContext _context;

  public:
    ParticleScene() = default;
    ~ParticleScene() override {};

    void Initialize() override;
    void Finalize() override;

  protected:
    void RegisterSystems() override;
    void OnDrawImGui() override;
};