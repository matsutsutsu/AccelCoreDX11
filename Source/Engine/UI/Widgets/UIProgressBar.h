#pragma once
#include "../UIElement.h"
#include "../UIManager.h" // Sprite取得のため必要


// ===========================================================================
// File: UIProgressBar.h
//
// 【役割】HPバーなどの「進捗ゲージ」ウィジェット
//
// 【解説】
// UIElementを継承し、画像の描画幅を動的に変更する機能を追加しています。
//
// SetProgress(0.0f ~ 1.0f) を呼ぶことで、画像の見た目の幅を伸縮させます。
// 左端を固定したまま幅を変えるための座標計算処理が含まれています。
// ===========================================================================


class UIProgressBar : public UIElement {
  private:
    float m_originalWidth; // 100%時の幅 (1920x1080基準)

  public:
    UIProgressBar(const std::string &name, const std::string &spriteName)
        : UIElement(name, spriteName), m_originalWidth(0.0f)
    {
    }

    // 初期化時に現在のサイズを最大幅として記憶
    void InitProgress() { m_originalWidth = m_size.x; }

    // 0.0 ~ 1.0 で進捗設定
    void SetProgress(float ratio)
    {
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        // 幅を変更 (座標はずらさない)
        m_size.x = m_originalWidth * ratio;
    }

    // 描画処理
    void OnRender(ID3D11DeviceContext *dc, float globalScale) override
    {
        if (!m_manager) return;
        Sprite *sprite = m_manager->GetSprite(m_spriteName);
        if (!sprite) return;

        // 現在の論理座標
        DirectX::XMFLOAT2 worldPos = GetWorldPosition();

        // 描画サイズ計算 (スケール適用)
        float drawW = m_size.x * m_scale * globalScale;
        float drawH = m_size.y * m_scale * globalScale;

        // 左詰め描画の計算ロジック
        // 本来の最大幅もスケールさせる
        float originalW_Scaled = m_originalWidth * m_scale * globalScale;

        // 中心座標から「最大幅の半分」を引いて、描画開始X座標(左端)を求める
        // これにより、幅(drawW)が縮んでも、左端の位置は固定される
        float drawX = (worldPos.x * globalScale) - (originalW_Scaled * 0.5f);
        float drawY = (worldPos.y * globalScale) - (drawH * 0.5f);


        // テクスチャの本来のピクセルサイズを取得
        float texWidth  = sprite->GetWidth();
        float texHeight = sprite->GetHeight();

        // 現在の割合（0.0 ~ 1.0）を計算
        float ratio = m_size.x / m_originalWidth;

        // 画像の切り抜きサイズを計算（幅だけを割合に合わせて減らす）
        float sw = texWidth * ratio;
        float sh = texHeight;


        // sx, sy, sw, sh を指定できる Render オーバーロードを使用！
        sprite->Render(dc,
            drawX,
            drawY,
            0.0f, // 描画先座標 (dx, dy, dz)
            drawW,
            drawH, // 描画サイズ (dw, dh)
            0.0f,
            0.0f, // 画像切り抜き開始位置 (sx, sy) = 左上から
            sw,
            sh, // 画像切り抜きサイズ (sw, sh)
            m_rotation,
            m_color.x,
            m_color.y,
            m_color.z,
            m_color.w);
    }
};