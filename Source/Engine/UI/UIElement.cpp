#include "UIElement.h"
#include "UIManager.h" // UIManagerの完全な定義が必要
#include <algorithm>
#include <cmath>

// 補間関数
template <typename T> static T Lerp(T start, T end, float t) { return start + (end - start) * t; }

UIElement::UIElement(const std::string &name, const std::string &spriteName)
    : m_name(name), m_spriteName(spriteName)
{
    // 初期ターゲットは現在値と同じにする
    m_targetPosition = m_localPosition;
    m_targetScale    = m_scale;
    m_targetColor    = m_color;
}

void UIElement::SetManager(UIManager *mgr)
{
    m_manager = mgr;
    m_isSpriteDirty = true; // マネージャーが変わったら取り直す

    // 子にも伝播させる
    for (auto &child : m_children) {
        child->SetManager(mgr);
    }
}

void UIElement::AddChild(std::shared_ptr<UIElement> child)
{
    if (!child) return;

    // 親子関係を結ぶ
    child->m_parent = this;

    // マネージャーを伝播
    if (m_manager) {
        child->SetManager(m_manager);
    }

    m_children.push_back(child);
}

DirectX::XMFLOAT2 UIElement::GetWorldPosition() const
{
    // 自分のローカル位置
    DirectX::XMFLOAT2 worldPos = m_localPosition;

    // 親がいるなら親のワールド位置を足す（再帰）
    if (m_parent) {
        DirectX::XMFLOAT2 parentPos = m_parent->GetWorldPosition();
        worldPos.x += parentPos.x;
        worldPos.y += parentPos.y;
    }
    return worldPos;
}

void UIElement::SetPosition(float x, float y)
{
    m_localPosition  = {x, y};
    m_targetPosition = {x, y};
}

void UIElement::MoveTo(float x, float y) { m_targetPosition = {x, y}; }

void UIElement::Update(float dt)
{
    // 1. 自身のアニメーション更新
    if (m_delayTimer > 0.0f) {
        m_delayTimer -= dt;
    }
    else {
        float t = 1.0f - std::expf(-m_smoothSpeed * dt);

        m_localPosition.x = Lerp(m_localPosition.x, m_targetPosition.x, t);
        m_localPosition.y = Lerp(m_localPosition.y, m_targetPosition.y, t);

        m_scale = Lerp(m_scale, m_targetScale, t);

        m_color.x = Lerp(m_color.x, m_targetColor.x, t);
        m_color.y = Lerp(m_color.y, m_targetColor.y, t);
        m_color.z = Lerp(m_color.z, m_targetColor.z, t);
        m_color.w = Lerp(m_color.w, m_targetColor.w, t);
    }

    // 2. 子要素の更新
    for (auto &child : m_children) {
        child->Update(dt);
    }
}

void UIElement::RenderRecursive(ID3D11DeviceContext *dc, float globalScale)
{
    if (!m_isVisible) return; // 見えないなら描画しないし、子供にも伝えない

    // 1. 自分を描画 (OnRenderは仮想関数)
    OnRender(dc, globalScale);

    // 2. 自分の子供全員に「お前たちも描画しろ」と命令する
    for (auto &child : m_children) {
        child->RenderRecursive(dc, globalScale); // ここがバケツリレー
    }
}

void UIElement::OnRender(ID3D11DeviceContext *dc, float globalScale)
{
    if (!m_manager) return;

    // 必要な時（初回や変更時）だけ倉庫に探しに行く
    if (m_isSpriteDirty) {
        m_cachedSprite  = m_manager->GetSprite(m_spriteName);
        m_isSpriteDirty = false;
    }

    // キャッシュしたポインタを直接使う（検索コスト・ゼロ！）
    if (!m_cachedSprite) return;

    // ワールド座標を取得（親の位置考慮済み）
    DirectX::XMFLOAT2 worldPos = GetWorldPosition();

    // 画面座標へ変換 (GlobalScale適用)
    float drawW = m_size.x * m_scale * globalScale;
    float drawH = m_size.y * m_scale * globalScale;

    float drawX = worldPos.x * globalScale;
    float drawY = worldPos.y * globalScale;

    // 中心基準で描画するためにオフセット
    drawX -= drawW * 0.5f;
    drawY -= drawH * 0.5f;

    m_cachedSprite->Render(dc,
        drawX,
        drawY,
        0.0f,
        drawW,
        drawH,
        m_rotation,
        m_color.x,
        m_color.y,
        m_color.z,
        m_color.w);
}

UIElement *UIElement::HitTestRecursive(float x, float y)
{
    if (!m_isVisible) return nullptr;

    // 子要素を手前（配列の後ろ）から順に判定
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        UIElement *hit = (*it)->HitTestRecursive(x, y);
        if (hit) return hit;
    }

    // 子に当たっていなければ自分を判定
    if (HitTest(x, y)) return this;

    return nullptr;
}

bool UIElement::HitTest(float x, float y)
{
    // 判定も「論理座標(1920x1080)」で行う
    // 引数の x, y は既に UIManager でスケール逆変換されている前提

    DirectX::XMFLOAT2 worldPos = GetWorldPosition();

    float halfW = (m_size.x * m_scale) * 0.5f;
    float halfH = (m_size.y * m_scale) * 0.5f;

    return (x >= worldPos.x - halfW && x <= worldPos.x + halfW && y >= worldPos.y - halfH &&
            y <= worldPos.y + halfH);
}