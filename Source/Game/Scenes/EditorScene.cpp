#include "EditorScene.h"
#include "Engine/Serialization/Factory/SceneInitializer.h"
#include "Engine/Graphics/Core/Graphics.h"
#include <imgui.h>

#include "Engine/Serialization/Factory/Prefab.h"

#include "Engine/Platform/Logger.h"

#include "ECS/System/CCL_SystemRegistry.h"

#include "Editor/Utils/NavMeshBuilder.h"

#include <ImGuizmo.h>
#include "Editor/Core/EditorCommandHistory.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

#include "Engine/GamePlay/Transform/TransformUpdateSystem.h"


void EditorScene::Initialize()
{
    BaseScene::Initialize(); // 共通初期化

    // エディタ固有の初期化
    _windowManager = std::make_unique<EditorWindowManager>();
    _windowManager->Initialize();


    ID3D11Device *device = Graphics::Instance().GetDevice();
    SceneInitializer::SpawnLightStartSet(*_worldPtr, device);
    //SceneInitializer::SpawnGameLightSetting(_world, device);
    //Prefab::SpawnPrefab(*_worldPtr, "Assets/Prefabs/Camera/FreeCamera.json");
    //Prefab::SpawnPrefab(*_worldPtr, "Assets/Prefabs/Camera/FreeFastCamera.json");

    //SceneManager::Instance().LoadScene(_worldPtr, "Assets/Scene/DroneEnemyTestScene.json");

    //SceneInitializer::SpawnTransformLoadTest(*_worldPtr, device, 100000);
    //SceneInitializer::SpawnJoltTest(*_worldPtr, device);

    // 中では普通にPBRモデル用にしている
    //SceneInitializer::SpawnAnimationTest(*_worldPtr, device);

    //_context.pendingLoadScenePath = "Assets/Scene/BossAITestScene.json";
    //_context.pendingLoadScenePath = "Assets/Scene/ChainAttackTestScene02.json";
    _context.pendingLoadScenePath = "Assets/Scene/HitStopDroneScene.json";

    //SceneInitializer::SpawnBossAndDronesTest(*_worldPtr, device);


    // ボスとドローン群のテスト環境を展開
    //SceneInitializer::SpawnBossAndDronesTest(*_worldPtr, device);


    //SceneInitializer::SpawnModelGridTest(*_worldPtr, device, 500);
    
    //SceneInitializer::SpawnPlayerCarTest(*_worldPtr, device);

    //SceneInitializer::SpawnShaderTest(*_worldPtr, device);

    // 例：10万個のキューブによる銀河の渦を生成
    //SceneInitializer::SpawnECSBenchmarkTest(*_worldPtr, device, 100000);

    // 300 x 300 = 90,000 個のキューブの海を生成
    //SceneInitializer::SpawnVoxelOceanTest(*_worldPtr, device, 300);
   
    //SceneInitializer::SpawnPhysicsAvalancheTest(*_worldPtr, device, 10000);

    // 1000体の群衆ストレステストを実行！
    //SceneInitializer::SpawnEnemySwarmTest(*_worldPtr, device, 200);

	//SceneInitializer::SpawnTornadoTest(*_worldPtr, device, 1000);
	//SceneInitializer::SpawnVectorFlowTest(*_worldPtr, device, 1000);
	//SceneInitializer::SpawnLissajousTest(*_worldPtr, device, 10000);


    CCL_LOG_INFO(LogCategory::Core, "EditorScene Initialize");
}

void EditorScene::Finalize() 
{
    BaseScene::Finalize(); // 共通終了化
    if (!_worldPtr->HasResource<UIManager*>()) return;
    auto* uiMgr = _worldPtr->GetResource<UIManager*>();

    _playerHUD = std::make_unique<PlayerHUD>();
    _playerHUD->Initialize(uiMgr);
}

void EditorScene::RegisterSystems()
{
    ID3D11Device *device = Graphics::Instance().GetDevice();

    // SystemManagerへのアクセスを _ecsContext 経由に変更
    // (SystemGroupsの関数が参照を受け取る仕様であると推測されるため、ポインタをデリファレンスしています)
    auto &sysManager = *(_ecsContext->GetSystemManager());
    CCL::ECS::SystemRegistry::Instance().RegisterAll(sysManager);

    // AnimationSystemを取得しておく (SystemGroups内で登録されている前提)
    _animationSystem = sysManager.GetSystem<AnimationSystem>();
}

void EditorScene::FixedUpdate(float dt) 
{
    // A. アニメーション編集モード中
    if (_context.isAnimEditMode && _context.selectedEntity != 0 &&
        _context.selectedEntity != CCL::ECS::InvalidEntityID) 
    {
      
        // ★重要: ゲーム全体（AIや重力など）は止めるが、
        // 階層（Transform）の更新だけは手動で回す！
        // これを行わないと、手動でアニメーションを動かした時に武器が追従せず画面が破綻する。
        if (_ecsContext && _ecsContext->GetSystemManager()) {
            auto* transformSystem = _ecsContext->GetSystemManager()->GetSystem<TransformUpdateSystem>();
            if (transformSystem) {
                transformSystem->Update(0.0f); // 0秒経過として親子関係だけを強制計算
            }
        }

        // BaseScene::FixedUpdate を呼ばずに return します。
        return;
    }

    // B. 通常プレイ中
    // 親クラスの処理を呼んで、通常通り世界を動かす
    BaseScene::FixedUpdate(dt);
}

// Updateのオーバーライド
void EditorScene::Update(float dt)
{
	// Contextからのリクエストを処理（ロード/セーブ要求など）
    ProcessContextRequests();

    UI_Update();

    // A. アニメーション編集モード中 (かつエンティティ選択中)
    if (_context.isAnimEditMode && _context.selectedEntity != 0 &&
        _context.selectedEntity != CCL::ECS::InvalidEntityID) {
        // 1. ゲームロジックは止める
        BaseScene::Update(0.0f);

        // 2. アニメーションだけ手動更新
        if (_animationSystem) {
            _animationSystem->UpdateManual(_context.selectedEntity, _context.animEditTime);
        }
    }
    // B. 通常プレイ中
    else {
        BaseScene::Update(dt);
    }
}

//シーンのみで使う更新処理描画やイベントの設定などはSystemに任せる
void EditorScene::UI_Update()
{
    // 4. ECSの掲示板(Resource)から最新データを取得し、UIに反映
    if (_worldPtr && _worldPtr->HasResource<PlayerHUDData>())
    {
        // 掲示板からデータを取得
        const auto& hudData = _worldPtr->GetResource<PlayerHUDData>();

        // 保持しているHUDクラスへデータを流し込む[cite: 21, 22]
        if (_playerHUD) {
            _playerHUD->ReflectData(hudData);
        }
    }
}



// GUI描画の拡張 (BaseScene::DrawGUIから呼ばれる)
void EditorScene::OnDrawImGui()
{
    _context.world         = _worldPtr;
    _context.systemManager = _ecsContext->GetSystemManager();


    _windowManager->Draw(_context);


    DrawGizmo();

    // ロード要求があればBaseSceneのパスに渡す
    if (!_context.pendingLoadScenePath.empty()) {
        _pendingLoadScenePath = _context.pendingLoadScenePath;
    }

    if (_windowManager->IsVisible()) {
        ImGui::Begin("System Info");
        _ecsContext->GetSystemManager()->OnGui();
        ImGui::End();
    }
}

void EditorScene::DrawGizmo()
{
    if (ImGui::IsKeyPressed(ImGuiKey_T)) _context.currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) _context.currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_Y)) _context.currentGizmoOperation = ImGuizmo::SCALE;


    if (_context.selectedEntity == CCL::ECS::InvalidEntityID) return;

    auto* transform = _worldPtr->GetComponent<TransformComponent>(_context.selectedEntity);
    if (!transform) return;

    // 1. 描画領域の設定
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // ★重要: バックグラウンド（3D空間）に確実に描画させるためのロック
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());

    // 2. BaseSceneのカメラから行列を取得
    DirectX::XMFLOAT4X4 viewFloat, projFloat;
    viewFloat = camera.GetView();
    projFloat = camera.GetProjection();

    // =========================================================
    // 3. ★アーキテクチャ修正: ギズモ用のスケール分離（Scale Stripping）
    // =========================================================
    DirectX::XMVECTOR gizmoScale;
    if (_context.currentGizmoOperation == ImGuizmo::SCALE) {
        // スケール操作の時だけ、本物のスケールをギズモに渡す
        gizmoScale = DirectX::XMLoadFloat3(&transform->scale);
    }
    else {
        // 移動・回転の時は、ギズモが歪んで当たり判定が壊れないように 1.0 に偽装する
        gizmoScale = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
    }

    DirectX::XMMATRIX worldMat = DirectX::XMMatrixAffineTransformation(
        gizmoScale,
        DirectX::XMVectorZero(),
        DirectX::XMLoadFloat4(&transform->rotation),
        DirectX::XMLoadFloat3(&transform->position)
    );
    DirectX::XMFLOAT4X4 worldFloat;
    DirectX::XMStoreFloat4x4(&worldFloat, worldMat);

    // 4. ギズモの描画と操作
    ImGuizmo::Manipulate(
        &viewFloat.m[0][0],
        &projFloat.m[0][0],
        _context.currentGizmoOperation,
        _context.currentGizmoMode,
        &worldFloat.m[0][0]
    );

    // =========================================================
    // 5. 状態検知と Undo/Redo
    // =========================================================
    static TransformComponent s_backupTransform;
    static bool s_isUsingGizmo = false;

    if (ImGuizmo::IsUsing()) {
        if (!s_isUsingGizmo) {
            s_backupTransform = *transform;
            s_isUsingGizmo = true;
            CCL_LOG_INFO(LogCategory::Editor, "[ImGuizmo] Drag started.");
        }

        float trans[3], rot[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(&worldFloat.m[0][0], trans, rot, scale);

        transform->position = { trans[0], trans[1], trans[2] };

        // ★修正: スケール操作中の時だけ、ギズモのスケール結果を適用する
        if (_context.currentGizmoOperation == ImGuizmo::SCALE) {
            transform->scale = { scale[0], scale[1], scale[2] };
        }

        DirectX::XMVECTOR quat = DirectX::XMQuaternionRotationRollPitchYaw(
            DirectX::XMConvertToRadians(rot[0]),
            DirectX::XMConvertToRadians(rot[1]),
            DirectX::XMConvertToRadians(rot[2])
        );
        DirectX::XMStoreFloat4(&transform->rotation, quat);

        transform->isDirty = true;

    }
    else {
        if (s_isUsingGizmo) {
            EditorCommandHistory::Instance().ExecuteCommand(
                std::make_unique<ChangeComponentCommand<TransformComponent>>(
                    _worldPtr, _context.selectedEntity, s_backupTransform, *transform
                )
            );
            CCL_LOG_INFO(LogCategory::Editor, "[ImGuizmo] Drag ended. Command issued.");
            s_isUsingGizmo = false;
        }
    }
}

// =========================================================
// UIからのリクエスト（予約）を処理する専用関数
// =========================================================
void EditorScene::ProcessContextRequests()
{
    if (!_context.pendingLoadScenePath.empty()) {
        RequestLoadScene(_context.pendingLoadScenePath);
        _context.pendingLoadScenePath.clear();

        //SceneInitializer::SpawnBossVFXTest(*_worldPtr);
    }

    if (!_context.pendingSaveScenePath.empty()) {
        RequestSaveScene(_context.pendingSaveScenePath);
        _context.pendingSaveScenePath.clear();
    }
}