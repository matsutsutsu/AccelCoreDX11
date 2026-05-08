#pragma once

#include <memory>
#include <string>

#include "Engine/Graphics/Core/Camera.h"
#include "Engine/Graphics/Core/Light.h"
#include "Engine/GamePlay/Core/Scene/Scene.h"

// ECS関連
#include "ECS/Common/CCL_Common.h"
#include "ECS/Core/CCL_PendingOps.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemManager.h"
#include "ECS/Core/CCL_ECSContext.h"

#include "Engine/Graphics/Shader/PostProcess/PostProcessManager.h"
//#include "Engine/Graphics/Shader/Pass/ShadowMap.h"

#include "Engine/GamePlay/Core/Scene/SceneManager.h"

class BaseScene : public Scene 
{
  protected:
    std::unique_ptr<CCL::ECS::ECSContext> _ecsContext;

    // 派生クラス（EditorScene等）の既存コードを壊さないための参照ポインタ
    CCL::ECS::Core::World *_worldPtr = nullptr;

    Camera             camera;
    LightManager       lightManager;
    PostProcessManager _postProcess;



    // セーブ・ロードの「予約」パス
    std::string _pendingLoadScenePath = "";
    std::string _pendingSaveScenePath = ""; 


  public:
    BaseScene()                   = default;
    virtual ~BaseScene() override = default;

    // 基本的なライフサイクル
    virtual void Initialize() override;
    virtual void Start() override; // GPUセットアップなど、Initialize後にメインスレッドで1回だけ呼ばれる処理
    virtual void Finalize() override;
    virtual void Update(float elapsedTime) override;
    virtual void FixedUpdate(float fixedTime) override;
    virtual void Render() override;  // 共通レンダリングパイプライン
    virtual void DrawGUI() override; // ImGuiエントリポイント

    // =========================================================
    // 外部（ImGui等）から呼ばれる「安全なリクエスト関数」
    // =========================================================
    void RequestLoadScene(const std::string& path) { _pendingLoadScenePath = path; }
    void RequestSaveScene(const std::string& path) { _pendingSaveScenePath = path; }
  protected:
    // --- 派生クラスでの拡張ポイント ---

    // システム登録 (必須)
    virtual void RegisterSystems() {}

    // ゲーム内UI描画 (Sprite, HealthBarなど)
    virtual void OnRenderUI() {}

    // ImGui描画 (Window, Inspectorなど)
    virtual void OnDrawImGui() {}

    // リクエストを消化する関数
    void ExecutePendingSceneOperations();

};