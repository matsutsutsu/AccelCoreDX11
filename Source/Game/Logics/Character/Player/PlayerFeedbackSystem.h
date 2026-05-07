#pragma once
#include "ECS/System/CCL_System.h"

// ===================================================================================
// クラス名: PlayerFeedbackSystem
// 役割: プレイヤーがダメージを受けた時などの「演出（フィードバック）」を専属で管理する
// ===================================================================================
class PlayerFeedbackSystem : public CCL::ECS::IfSystem<PlayerFeedbackSystem> {
  public:
    PlayerFeedbackSystem();
    virtual ~PlayerFeedbackSystem() = default;

    // ここで EventBus の購読（Subscribe）を設定します
    void Initialize() override;

    // イベント駆動システムなので基本的に空ですが、時間経過の演出（フェードアウト等）に使えます
    void Update(float dt) override;
};