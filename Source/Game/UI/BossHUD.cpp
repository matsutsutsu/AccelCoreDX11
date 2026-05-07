#include "BossHUD.h"
#include "ImGui.h"

void BossHUD::Initialize(UIManager* uiManager)
{
    if (!uiManager) return;

    // ★ 注意: テクスチャのパスは実際のプロジェクトのものに合わせてください
    std::wstring boardPath = L"Data/Sprite/UIBar/BossBord.png";
    std::wstring hpBarPath = L"Data/Sprite/UIBar/BossHPBar.png";

    uiManager->LoadSprite("Tex_BossBord", boardPath.c_str());
    uiManager->LoadSprite("Tex_BossHP_Fill", hpBarPath.c_str());

    // 1. ルート要素
    m_rootElement = std::make_shared<UIElement>();
    m_rootElement->SetName("BossHUD_Root");
    uiManager->AddElement(m_rootElement);

    // 2. ボス用ボード（装飾など）
    m_bossBoard = std::make_shared<UIElement>("HUD_BossBord", "Tex_BossBord");
    m_bossBoard->SetSize(1200.0f, 100.0f);
    // 画面上部の中央に配置 (1920x1080基準)
    m_bossBoard->SetPosition(960.0f, 100.0f);
    m_bossBoard->SetScale(1.0f);
    m_rootElement->AddChild(m_bossBoard);

    // 3. ボスHPバー（進捗ゲージ）
    m_hpBar = std::make_shared<UIProgressBar>("HUD_BossHPBar", "Tex_BossHP_Fill");
    m_hpBar->SetColor(1.0f, 0.0f, 0.0f, 1.0f); // 赤色
    m_hpBar->SetSize(1100.0f, 40.0f);
    m_hpBar->SetPosition(0.0f, 0.0f); // ボードに対する相対座標
    m_hpBar->InitProgress();
    m_bossBoard->AddChild(m_hpBar);
}

void BossHUD::ReflectData(const BossHUDData& data)
{
    if (m_rootElement) {
        m_rootElement->SetVisible(data.isVisible);
    }
    if (m_hpBar) {
        m_hpBar->SetProgress(data.hpRatio);

        // おまけ演出: 第2フェーズに入ったらHPバーの色を紫色に変えるなど
        if (data.currentPhase >= 2) {
            m_hpBar->SetColor(0.8f, 0.0f, 1.0f, 1.0f);
        }
    }
}

void BossHUD::Gui()
{
    ImGui::Begin("Boss HUD Editor");
    if (m_rootElement) {
        m_rootElement->OnDebugGUI();
    }
    ImGui::End();
}