#pragma once
#include "../UIElement.h"
#include <functional>

// ===========================================================================
// File: SimpleButton.h
//
// 【役割】クリック可能な「ボタン」ウィジェット
//
// 【解説】
// UIElementを継承し、以下の機能を追加したクラスです。
//
// 1. ホバー演出:
//    マウスカーソルが乗った時に、少し拡大したり色を明るくするアニメーションを行います。
//
// 2. クリック動作:
//    コンストラクタで「押されたら実行する関数（ラムダ式など）」を受け取り、
//    クリック時にそれを実行します。
// ===========================================================================

// クリック時の動作を自由に設定できるボタン
class SimpleButton : public UIElement {
  private:
    std::function<void()> m_onClickAction; // クリック時に実行されるコールバック

  public:
    // コンストラクタ
    SimpleButton(
        const std::string &name, const std::string &spriteName, std::function<void()> onClick)
        : UIElement(name, spriteName), m_onClickAction(onClick)
    {
    }

    // --- ホバー演出 ---
    // 親クラスの m_baseScale を基準にアニメーションさせるため、
    // サイズが勝手にリセットされる問題を回避しています。

    void OnHoverEnter() override
    {
        UIElement::OnHoverEnter();
        //m_color = {1.2f, 1.2f, 1.2f, 1.0f}; // 明るくする
        //m_scale = m_baseScale * 1.1f;       // 基準サイズに対して1.1倍
        
        //　アニメーション
        //  目標値をセットするだけ。あとはUpdateが勝手に動かしてくれる。
        ColorTo(1.2f, 1.2f, 1.2f, 1.0f);
        ScaleTo(m_baseScale * 1.1f);
    }

    void OnHoverExit() override
    {
        UIElement::OnHoverExit();
        //m_color = {1.0f, 1.0f, 1.0f, 1.0f}; // 元に戻す
        //m_scale = m_baseScale;              // 基準サイズに戻す

        // 元のサイズ・色を目指して戻る
        ColorTo(1.0f, 1.0f, 1.0f, 1.0f);
        ScaleTo(m_baseScale);
    }

    // --- クリック動作 ---
    void OnClick() override
    {
        // 登録された関数を実行
        if (m_onClickAction) {
            m_onClickAction();
        }
    }
};