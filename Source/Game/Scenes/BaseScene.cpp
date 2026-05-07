#include "BaseScene.h"
#include <imgui.h>


#include "Engine/Serialization/SceneSerializer.h"
#include "Engine/Serialization/Factory/Prefab.h"
#include "Engine/Serialization/Factory/SceneInitializer.h"

#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Physics/JoltPhysicsManager.h"

#include "Engine/Physics/JoltPhysicsFacade.h"
#include "Engine/Audio/FmodAudioManager.h"

#include "Engine/Platform/Input/Input.h"
#include "Engine/Platform/Input/InputFacade.h"

#include "Engine/GamePlay/Graphics/Core/EnvironmentSystem.h"

#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "Engine/Graphics/Shader/ShaderResources.h"

#include "Engine/GamePlay/Core/Time/TimeState.h"

#include "tracy/Tracy.hpp"

#include "Engine/Platform/Logger.h"


void BaseScene::Initialize()
{
    ZoneScopedN("BaseScene::Initialize");


    auto &graphics = Graphics::Instance();

    ID3D11Device *device       = graphics.GetDevice();
    float         screenWidth  = graphics.GetScreenWidth();
    float         screenHeight = graphics.GetScreenHeight();


    // カメラ基本設定
    camera.SetPerspectiveFov(
        DirectX::XMConvertToRadians(45), screenWidth / screenHeight, 0.1f, 1000.0f);
    camera.SetLookAt({0, 10, -10}, {0, 0, 0}, {0, 1, 0});

    lightManager.Initialize(device);
    Prefab::Initialize(device);

    // ECSコンテキストの生成
    _ecsContext = std::make_unique<CCL::ECS::ECSContext>();
    _worldPtr   = _ecsContext->GetWorld(); // 互換性用

    // リソース登録
    _worldPtr->AddResource<Input *>(&Input::Instance());

    auto inputAPI = std::make_shared<InputFacade>(&Input::Instance());

    // C++に長々と書いていた割り当てが、この1行に集約される！
    inputAPI->LoadConfig("Assets/Config/InputConfig.json");

    _worldPtr->AddResource<std::shared_ptr<IInputAPI>>(inputAPI);

    // 1. FMODの生成と初期化
    auto audioAPI = std::make_shared<FmodAudioManager>();
    audioAPI->Initialize();

    // 2. 設定ファイルからのデータ駆動ロード（1行に集約！）
    audioAPI->LoadConfig("Assets/Config/AudioConfig.json");

    // 3. ECSの世界に登録
    _worldPtr->AddResource<std::shared_ptr<IAudioAPI>>(audioAPI);



    // WorldのリソースとしてJoltを登録し、初期化する
    _worldPtr->AddResource<JoltPhysicsManager>();
    _worldPtr->GetResource<JoltPhysicsManager>().Initialize();

    // 箱の型として「IPhysicsAPIのshared_ptr」を明示し、中身として「Joltのshared_ptr」を渡す
    auto myFacade = std::make_shared<JoltPhysicsFacade>(_worldPtr);
    _worldPtr->AddResource<std::shared_ptr<IPhysicsAPI>>(myFacade);

    // システムがコンストラクタ引数なしでもアクセスできるようにポインタを登録
    _worldPtr->AddResource<Camera *>(&camera);
    _worldPtr->AddResource<LightManager *>(&lightManager);
    _worldPtr->AddResource<PostProcessManager *>(&_postProcess);
	_worldPtr->AddResource<ShadowMap*>(graphics.GetShadowMap());

    _worldPtr->AddResource<ModelRenderer *>(graphics.GetModelRenderer());
    _worldPtr->AddResource<ParticleRenderer *>(graphics.GetParticleRenderer());
    _worldPtr->AddResource<PrimitiveRenderer *>(graphics.GetPrimitiveRenderer());
    _worldPtr->AddResource<ShapeRenderer *>(graphics.GetShapeRenderer());

    _worldPtr->AddResource<TimeContext>();


    // TriggerSystem 等がアクセスできるように、SystemManager のポインタをリソースとして登録
    _worldPtr->AddResource<CCL::ECS::SystemManager *>(_ecsContext->GetSystemManager());

    // システム登録 (派生クラスの処理が走る)
    RegisterSystems();

    _postProcess.Initialize(device, (int)screenWidth, (int)screenHeight);

    // GPUパーティクル事前準備　オブジェクトプール用に最初に確保しておく
    Prefab::PrewarmParticles(*_worldPtr);

    
   
}

void BaseScene::Start() {}

void BaseScene::Finalize() {}

void BaseScene::Update(float elapsedTime)
{
    ZoneScopedN("BaseScene::Update");


    // ECSシステムが走り出す「前」の、誰もメモリを触っていない安全な瞬間に世界を切り替える！
    ExecutePendingSceneOperations();

    RenderQueue::Instance().BeginFrame();
    Graphics::Instance().GetParticleRenderer()->BeginFrame();

    // Graphics定数バッファ更新 (EditorSceneにあった処理を共通化)
    Graphics::Instance().Update(&camera);

    if (_ecsContext) {
       _ecsContext->UpdateRender(elapsedTime);
    }

}

void BaseScene::FixedUpdate(float fixedTime) 
{ 
    ZoneScopedN("BaseScene::FixedUpdate");


    // ECSコンテキストに更新を委譲（これだけで全てが正しい順序で回る）
    if (_ecsContext) {
        _ecsContext->UpdateLogic(fixedTime);
    }
}

void BaseScene::Render()
{
    ZoneScopedN("BaseScene::Render Pipeline"); // シーン描画全体


    auto &graphics = Graphics::Instance();
    auto *dc       = graphics.GetDeviceContext();

    // 1. 描画準備 & ポストプロセス開始
    graphics.SetRenderTargets(false);
 

    // 2. 3D描画コンテキスト設定
    RenderContext rc;
    rc.deviceContext = dc;
    rc.renderState   = graphics.GetRenderState();
    rc.camera        = &camera;
    rc.lightManager  = &lightManager;
    rc.shadowMap     = graphics.GetShadowMap();

    rc.modelRenderer = graphics.GetModelRenderer();
    rc.renderQueue = &RenderQueue::Instance();
    rc.postProcess = &_postProcess;
    rc.particleRenderer = graphics.GetParticleRenderer();
    rc.primitiveRenderer = graphics.GetPrimitiveRenderer();
    rc.shapeRenderer = graphics.GetShapeRenderer();



    // =========================================================
    // 3. 舞台監督によるグローバルリソースのセット
    // =========================================================
    if (_ecsContext && _ecsContext->GetSystemManager()) {
        if (auto *envSystem = _ecsContext->GetSystemManager()->GetSystem<EnvironmentSystem>()) {
            // ここで CbScene の転送、フォグ、ノイズがセットされる
            envSystem->BindGlobalResources(dc);
        }
    }

    // =========================================================
    // 4. ライトのバインド
    // =========================================================
    if (rc.lightManager) {
        rc.lightManager->Bind(dc, SLOT_CB_LIGHT);
    }


    // =========================================================
    // 4. パイプラインの一括実行！ (これ1行ですべての3D描画とポストプロセスが完了する)
    // =========================================================
    if (rc.modelRenderer) {
        rc.modelRenderer->Render(rc);
    }
 
   

    // -----------------------------------------------------------------
    // 【フェーズ5：2D UI描画】
    // -----------------------------------------------------------------
    {
        ZoneScopedN("5. UI Render");


        dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xffffffff);

        // =========================================================================
        // 深度テストを完全にオフにする（DepthState::None）
        // これにより、Depthが「No Resource」であってもGPUはエラーを出さず、
        // UIや文字のピクセルをすべて無条件で画面に焼き付けます。
        // =========================================================================
        dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        // ゲーム内UI (Spriteなど)
        OnRenderUI();

    }
}

void BaseScene::DrawGUI()
{
    // ★拡張ポイント: ImGui描画
    OnDrawImGui();
}



// ---------------------------------------------------------
// 予約されたセーブ/ロード処理の消化 (フレームの先頭で呼ばれる)
// ---------------------------------------------------------
void BaseScene::ExecutePendingSceneOperations()
{
    // ロード予約がある場合
    if (!_pendingLoadScenePath.empty()) {
        // ★必要であればここでパーティクル等の事前リセット処理 (OnPreLoadScene) を呼ぶ

        SceneManager::Instance().LoadScene(_worldPtr, _pendingLoadScenePath);
        _pendingLoadScenePath.clear();
    }

    // セーブ予約がある場合
    if (!_pendingSaveScenePath.empty()) {
        SceneManager::Instance().SaveScene(_worldPtr, _pendingSaveScenePath);
        _pendingSaveScenePath.clear();
    }
}