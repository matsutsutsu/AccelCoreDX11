#pragma once
#include "Editor/Core/EditorWindowManager.h"
#include "Game/Scenes/BaseScene.h"


#include "Engine/GamePlay/Animation/AnimationSystem.h"

class EditorScene : public BaseScene {
    // エディタ固有
    std::unique_ptr<EditorWindowManager> _windowManager;

    // アニメーションシステムへの参照 (手動更新用)
    AnimationSystem* _animationSystem;


    // エディタの全記憶を保持するコンテキスト（毎フレーム破棄されない）
    EditorContext _context;

  public:
    EditorScene() = default;
    ~EditorScene() override {};

    void Initialize() override;
    void Finalize() override;
    // Finalize, Update, Render, FixedUpdate は BaseScene をそのまま使うため削除！

  protected:
    void RegisterSystems() override;

    // BaseSceneのUpdateをオーバーライドする
    void FixedUpdate(float dt) override;
    void Update(float dt) override;

    void OnDrawImGui() override;

    void DrawGizmo();

    //  UIコンテキストからの要求を解釈し、BaseSceneに流し込む専用関数
    void ProcessContextRequests();
};