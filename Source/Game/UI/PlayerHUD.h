#pragma once

#pragma once
#include <memory>
#include <string>

// エンジン側のUI基盤をインクルード
#include "Engine/UI/UIManager.h"
#include "Engine/UI/UIElement.h"
#include "Engine/UI/Widgets/UIProgressBar.h"

// ECSコンポーネントそのものではなく、UIが表示に必要な「加工済みの値」だけを持つ
struct PlayerHUDData {
    float hpRatio = 1.0f; // 0.0 ~ 1.0
    float staminaRatio = 1.0f;
    float specialGaugeRatio = 1.0f;
};

class PlayerHUD
{
public:
    PlayerHUD() = default;
    ~PlayerHUD() = default;

    // UIの構築（SceneGame::Startから呼ぶ）
    void Initialize(UIManager* uiManager);

    // データの反映（SceneGame::Updateから呼ぶ）
    void ReflectData(const PlayerHUDData& data);

    // 表示・非表示の切り替え（演出用）
    void SetVisible(bool visible);

    void Gui();
private:
    // 管理するパーツ群
    std::shared_ptr<UIElement> m_PlayerBord;
    std::shared_ptr<UIProgressBar> m_hpBar;
    std::shared_ptr<UIProgressBar> m_staminaBar;
    std::shared_ptr<UIElement>     m_rootElement; // 全パーツの親（一括操作用）
};


