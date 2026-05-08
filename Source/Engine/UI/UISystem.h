#pragma once
// Engine/UI/UISystem.h
#include "ECS/System/CCL_System.h"
#include <memory>

class UIManager;
class PlayerHUD;

class UISystem : public CCL::ECS::SystemBase
{
public:
    UISystem();
    virtual ~UISystem() = default;

    // UI更新に必要なリソース（UIManager等）をチェック
    std::vector<CCL::ECS::TypeID> GetReadTypes() const override;

    void Initialize() override;

    // シーンに書かれていたロジックをここに集約
    void Update(float dt) override;

    void ProcessUIEvents(UIManager* uiMgr);

    // 描画フェーズ（RenderPacketへの積み込み）を担当
    void RenderUI();
};