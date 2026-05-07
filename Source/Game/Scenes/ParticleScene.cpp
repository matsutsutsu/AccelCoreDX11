#include "ParticleScene.h"
#include "Engine/Serialization/Factory/Prefab.h"
#include "Engine/Graphics/Core/Graphics.h"
#include <imgui.h>
#include "Engine/Serialization/Factory/SceneInitializer.h"

#include "ECS/System/CCL_SystemRegistry.h"

void ParticleScene::Initialize()
{
    BaseScene::Initialize();

    _windowManager = std::make_unique<EditorWindowManager>();
    _windowManager->Initialize();

    ID3D11Device *device = Graphics::Instance().GetDevice();
    SceneInitializer::SpawnLightStartSet(*_worldPtr, device);

    EntityID camera = Prefab::SpawnPrefab(*_worldPtr, "Assets/Prefabs/Camera/FreeCamera");



    // ParticleScene固有: 特定のパーティクルを生成
    Prefab::SpawnPrefab(*_worldPtr, "Assets/Prefabs/Particles/VFX_Scene_Start");
}

void ParticleScene::Finalize()
{
    BaseScene::Finalize(); 
}

void ParticleScene::RegisterSystems()
{
    // EditorSceneと全く同じ (SystemGroupsのおかげで短縮)
    ID3D11Device *device = Graphics::Instance().GetDevice();

    // SystemManagerへのアクセスを _ecsContext 経由に変更
    // (SystemGroupsの関数が参照を受け取る仕様であると推測されるため、ポインタをデリファレンスしています)
    auto &sysManager = *(_ecsContext->GetSystemManager());
    CCL::ECS::SystemRegistry::Instance().RegisterAll(sysManager);
}


void ParticleScene::OnDrawImGui()
{
    _context.world         = _worldPtr;
    _context.systemManager = _ecsContext->GetSystemManager();

    if (_windowManager) {
        _windowManager->Draw(_context);
    }

    if (!_context.pendingLoadScenePath.empty()) {
        _pendingLoadScenePath = _context.pendingLoadScenePath;
    }

    if (_windowManager->IsVisible()) {
        ImGui::Begin("System Info");
        _ecsContext->GetSystemManager()->OnGui();
        ImGui::End();
    }

}

