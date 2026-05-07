#pragma once
#include <memory>
#include <string>
#include "Engine/UI/UIManager.h"
#include "Engine/UI/UIElement.h"
#include "Engine/UI/Widgets/UIProgressBar.h"
#include "BossHUDData.h"

class BossHUD {
public:
    BossHUD() = default;
    ~BossHUD() = default;

    void Initialize(UIManager* uiManager);
    void ReflectData(const BossHUDData& data);
    void Gui();

private:
    std::shared_ptr<UIElement>     m_rootElement;
    std::shared_ptr<UIElement>     m_bossBoard;
    std::shared_ptr<UIProgressBar> m_hpBar;
};