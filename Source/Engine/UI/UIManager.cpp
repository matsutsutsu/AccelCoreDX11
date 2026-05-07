#include "UIManager.h"
#include "Engine/Graphics/Core/Graphics.h" // 画面サイズ取得用
#include "UIContext.h"
#include "UIElement.h"
#include <imgui.h>
#include <iostream>

// ===========================================================================
// File: UIManager.h / .cpp
//
// 【役割】UIシステムの「総監督」兼「座標の翻訳家」
//
// 【解説】
// 1. 画面サイズ管理:
//    実際の画面サイズ（1280x720など）と、UI設計サイズ（1920x1080）の比率を計算し、
//    UI全体を自動で拡大縮小（グローバルスケール）させます。
//
// 2. 一括管理:
//    すべてのUI要素（ルート要素）をリストで持ち、一括で更新・描画・削除を行います。
//
// 3. 入力処理:
//    マウス座標を「UI用の座標」に変換して、各UI要素にヒットテスト（当たり判定）を行います。
// ===========================================================================

UIManager::UIManager(ID3D11Device *device, ID3D11DeviceContext *context)
    : m_device(device), m_context(context)
{
}

UIManager::~UIManager() { ClearElements(); }

float UIManager::GetGlobalScale() const
{
    const float baseHeight    = 1080.0f;    // 設計時の基準（フルHD）
    float       currentHeight = (float)Graphics::Instance().GetScreenHeight();
    return currentHeight / baseHeight;
}

void UIManager::LoadSprite(const std::string &name, const char *filepath)
{
    m_sprites[name] = std::make_unique<Sprite>(m_device, filepath);
}

Sprite *UIManager::GetSprite(const std::string &name) const
{
    auto it = m_sprites.find(name);
    if (it != m_sprites.end()) return it->second.get();
    return nullptr;
}

void UIManager::AddElement(std::shared_ptr<UIElement> element)
{
    if (!element) return;

    // マネージャーをセット
    element->SetManager(this);

    // ルートリストに追加
    m_rootElements.push_back(element);
}

void UIManager::ClearElements()
{
    m_rootElements.clear();
    m_pHoveredElement = nullptr;
    m_pDragSource     = nullptr;
}

void UIManager::Update(float dt, const UIMouseState &mouse)
{
    // 1. スケール計算
    float globalScale = GetGlobalScale();

    // 2. マウス座標を「論理座標 (1920x1080)」に変換
    // これにより、UIElement側は画面解像度を気にせず 1920x1080 前提で判定できる
    float logicalMouseX = mouse.x / globalScale;
    float logicalMouseY = mouse.y / globalScale;

    // 3. 全要素の更新 (ルートから再帰的に行われる)
    for (auto &e : m_rootElements) {
        e->Update(dt);
    }

    // 4. ヒットテスト (ルート要素を手前から順にチェック)
    UIElement *newHover = nullptr;

    for (auto it = m_rootElements.rbegin(); it != m_rootElements.rend(); ++it) {
        // 再帰ヒットテスト
        UIElement *hit = (*it)->HitTestRecursive(logicalMouseX, logicalMouseY);
        if (hit) {
            newHover = hit;
            break;
        }
    }

    // 5. ホバー状態の更新
    if (newHover != m_pHoveredElement) {
        if (m_pHoveredElement) m_pHoveredElement->OnHoverExit();
        if (newHover) newHover->OnHoverEnter();
        m_pHoveredElement = newHover;
    }

    // 6. クリック処理
    if (m_pHoveredElement) {
        if (mouse.isPressed) {
            m_pHoveredElement->OnPress();
        }
        else if (mouse.isReleased) {
            m_pHoveredElement->OnRelease();
            m_pHoveredElement->OnClick();
        }
    }
}

void UIManager::Render()
{
    float globalScale = GetGlobalScale();

    // ルート要素から順に描画
    for (auto &e : m_rootElements) {
        e->RenderRecursive(m_context, globalScale);
    }
}

void UIManager::DrawDebugGUI()
{
    if (ImGui::Begin("UI Manager Debug")) {
        ImGui::Text("Global Scale: %.2f", GetGlobalScale());
        ImGui::Text("Root Elements: %d", (int)m_rootElements.size());

        if (m_pHoveredElement) {
            ImGui::Text("Hover: %s", m_pHoveredElement->GetName().c_str());
        }
        else {
            ImGui::Text("Hover: None");
        }
    }
    ImGui::End();
}