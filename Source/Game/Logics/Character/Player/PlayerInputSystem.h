#pragma once
#include "ECS/System/CCL_System.h"                  // IfSystemの継承元
#include "Game/Logics/Character/Player/PlayerComponent.h" // テンプレート引数で必要
#include "Game/Logics/GameEvent/GameStateMode.h"  //ゲームのステート状態

// 前方宣言 (インクルード削減のため)
class Camera;

class PlayerInputSystem
    : public CCL::ECS::IfSystem<PlayerInputSystem, CCL::ECS::Write<PlayerComponent>> {
  public:
    // コンストラクタ
    PlayerInputSystem();
    virtual ~PlayerInputSystem() = default;

    // 更新処理
    void Update(float dt) override;
};