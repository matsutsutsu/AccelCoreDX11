#include "PlayerHUD.h"
#include "ImGui.h"

void PlayerHUD::Initialize(UIManager* uiManager)
{
    if (!uiManager)return;

    std::wstring BerFrameFilePath = L"Data/Sprite/UIBar/PlayerBord.png";
    std::wstring HPBerFilePath = L"Data/Sprite/UIBar/HealhtBar.png";
    std::wstring StaminaBerFilePath = L"Data/Sprite/UIBar/Stamina.png";
    uiManager->LoadSprite("Tex_PlayerBord", BerFrameFilePath.c_str());
    uiManager->LoadSprite("Tex_HP_Fill", HPBerFilePath.c_str());
    uiManager->LoadSprite("Tex_Stamina_Fill", StaminaBerFilePath.c_str());

    // --- 1. ルート要素の作成 ---
    m_rootElement = std::make_shared<UIElement>();
    m_rootElement->SetName("PlayerHUD_Root");
    uiManager->AddElement(m_rootElement);

    // --- 2. 灰色ボード(土台)の作成 ---
    m_PlayerBord = std::make_shared<UIElement>("HUD_PlayerBord", "Tex_PlayerBord");
    m_PlayerBord->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    m_PlayerBord->SetSize(1600.0f, 600.0f);
    m_PlayerBord->SetPosition(425.000f, 200.000f);
    m_PlayerBord->SetScale(0.5f);
    m_rootElement->AddChild(m_PlayerBord);

    // --- 3. HPバー(緑)の作成 ---
    m_hpBar = std::make_shared<UIProgressBar>("HUD_HPBar", "Tex_HP_Fill");
    m_hpBar->SetColor(0.2f, 1.0f, 0.2f, 1.0f);
    m_hpBar->SetSize(610.0f, 55.0f);
    m_hpBar->SetScale(0.8);

    // 【修正ポイント】土台ボードからの相対位置
    m_hpBar->SetPosition(110.0f, 5.0f);
    m_hpBar->InitProgress();

    m_PlayerBord->AddChild(m_hpBar);

    // --- 4. スタミナバー(黄)の作成 ---
    m_staminaBar = std::make_shared<UIProgressBar>("HUD_StaminaBar", "Tex_Stamina_Fill");
    m_staminaBar->SetColor(1.0f, 1.0f, 0.0f, 1.0f);
    m_staminaBar->SetSize(540.0f, 40.0f);
    m_staminaBar->SetPosition(80.0f, 70.0f);
    m_staminaBar->SetScale(0.8);
    m_staminaBar->InitProgress();



    m_PlayerBord->AddChild(m_staminaBar);
};




void PlayerHUD::ReflectData(const PlayerHUDData& data)
{
    // ECSシステムが書き込んだ「加工済みデータ」をUIに流し込む
    if (m_hpBar) {
        m_hpBar->SetProgress(data.hpRatio);
    }

    if (m_staminaBar) {
        m_staminaBar->SetProgress(data.staminaRatio);
    }
}

void PlayerHUD::SetVisible(bool visible)
{
    if (m_rootElement) 
    {
        m_rootElement->SetVisible(visible);
    }
}

void PlayerHUD::Gui()
{
    ImGui::Begin("HUD Editor");
    if (m_rootElement) {
        m_rootElement->OnDebugGUI();
    }

    ImGui::End();
}