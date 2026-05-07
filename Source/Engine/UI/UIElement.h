#pragma once
#include "UIContext.h"
#include <DirectXMath.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ===========================================================================
// File: UIElement.h / .cpp
//
// 【役割】すべてのUIウィジェットの「親玉（基底クラス）」
//
// 【解説】
// UIの基本機能をすべて詰め込んだクラスです。
//
// 1. 階層構造（親子関係）:
//    「親」と「子」の概念を持ち、親が動けば子も追従して動く仕組みを提供します。
//    座標はすべて「親からの相対位置（ローカル座標）」で管理されます。
//
// 2. 座標計算:
//    ローカル座標から、画面上の実際の場所（ワールド座標）を計算するロジックを持ちます。
//
// 3. 再帰処理:
//    「自分を描画したら、次は子供たちを描画させる」という再帰的な更新・描画を行います。
// ===========================================================================


// 前方宣言
class UIManager;
class Sprite;
struct ID3D11DeviceContext;

// std::enable_shared_from_this: 自分のshared_ptrを作成するために継承
class UIElement : public std::enable_shared_from_this<UIElement> {
    // Managerがprivateメンバにアクセスできるようにする
    friend class UIManager;

  protected:
    UIManager *m_manager = nullptr; // マネージャーへの参照

    // --- 階層構造 ---
    UIElement                              *m_parent = nullptr;
    std::vector<std::shared_ptr<UIElement>> m_children;

    // --- 識別 ---
    std::string m_name;
    std::string m_spriteName;

    Sprite *m_cachedSprite  = nullptr; // ポインタのメモ
    bool    m_isSpriteDirty = true;    // メモを書き直す必要があるか

    // --- 座標 (親からの相対座標・1920x1080基準) ---
    DirectX::XMFLOAT2 m_localPosition = {0, 0};
    DirectX::XMFLOAT2 m_size          = {100, 100};
    float             m_rotation      = 0.0f;

    // --- スケール・色 ---
    float             m_baseScale = 1.0f;
    float             m_scale     = 1.0f;
    DirectX::XMFLOAT4 m_color     = {1, 1, 1, 1};

    // --- 状態 ---
    bool m_isVisible = true;
    bool m_isHovered = false;
    bool m_isPressed = false;

    // --- アニメーション用 (Damping) ---
    DirectX::XMFLOAT2 m_targetPosition = {0, 0};
    float             m_targetScale    = 1.0f;
    DirectX::XMFLOAT4 m_targetColor    = {1, 1, 1, 1};
    float             m_smoothSpeed    = 15.0f;
    float             m_delayTimer     = 0.0f;

  public:
    UIElement(const std::string &name, const std::string &spriteName);
    virtual ~UIElement() = default;

    // --- 階層操作 ---
    void       AddChild(std::shared_ptr<UIElement> child);
    UIElement *GetParent() const { return m_parent; }

    // --- 座標計算 ---
    // ワールド座標（親の座標を加算した最終位置）を取得
    DirectX::XMFLOAT2 GetWorldPosition() const;

    // --- 更新・描画 ---
    // Updateは再帰的に呼ばれる
    virtual void Update(float dt);

    // 描画エントリポイント (再帰処理)
    void RenderRecursive(ID3D11DeviceContext *dc, float globalScale);

    // 実際の描画処理 (派生クラスでオーバーライドする)
    virtual void OnRender(ID3D11DeviceContext *dc, float globalScale);

    // --- 判定 ---
    // 再帰的にヒットテストを行う
    UIElement *HitTestRecursive(float x, float y);

    // 自身の判定
    virtual bool HitTest(float x, float y);

    // --- 操作系 ---
    void SetPosition(float x, float y); // ローカル座標セット
    void MoveTo(float x, float y);      // ローカル座標へ移動

    // スケールを即時設定する
    void SetScale(float s)
    {
        m_baseScale   = s;
        m_scale       = s;
        m_targetScale = s;
    }

    // 色を即時設定する
    void SetColor(float r, float g, float b, float a)
    {
        m_color       = {r, g, b, a};
        m_targetColor = {r, g, b, a};
    }

    void ScaleTo(float s) { m_targetScale = s; }
    void ColorTo(float r, float g, float b, float a) { m_targetColor = {r, g, b, a}; }

    void SetSize(float w, float h) { m_size = {w, h}; }
    void SetDelay(float s) { m_delayTimer = s; }

    bool               IsVisible() const { return m_isVisible; }
    void               SetVisible(bool v) { m_isVisible = v; }
    const std::string &GetName() const { return m_name; }

    // --- イベント (派生クラス用) ---
    virtual void OnHoverEnter() { m_isHovered = true; }
    virtual void OnHoverExit()
    {
        m_isHovered = false;
        m_isPressed = false;
    }
    virtual void OnPress() { m_isPressed = true; }
    virtual void OnRelease() { m_isPressed = false; }
    virtual void OnClick() {}

    // ドラッグ系（今回は簡易化のためデフォルトfalse）
    virtual bool IsDraggable() const { return false; }

    // マネージャーセット（AddChild時に自動で呼ばれる）
    void SetManager(UIManager *mgr);
};