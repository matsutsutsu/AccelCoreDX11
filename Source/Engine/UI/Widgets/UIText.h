#pragma once
#include "../UIElement.h"
#include "../UIManager.h" // Manager経由でTextManagerにアクセスするため必要


// ===========================================================================
// File: UIText.h
//
// 【役割】テキスト（文字）をUIシステム内で扱うためのウィジェット
//
// 【解説】
// 本来UIシステムとは独立している「TextManager」の描画機能を、
// UIElementの仕組み（親子関係・座標系）の中で使えるようにしたクラスです。
//
// これを使うことで、「ウィンドウ（親）が動いたら、中の文字（子）もついてくる」
// といった挙動が可能になります。
// ===========================================================================


class UIText : public UIElement {
  private:
    std::string m_textString;
    std::u32string m_cachedUTF32;        // 翻訳済みの文字データを保存
    bool           m_isTextDirty = true; // 文字が変わったかどうかのフラグ
    bool        m_isCentered = true;

  public:
    // コンストラクタ: SpriteNameは空文字でOK
    UIText(const std::string &name, const std::string &text)
        : UIElement(name, ""), m_textString(text)
    {
    }

    void SetText(const std::string &text)
    {
        // 違う文字が来た時だけ更新フラグを立てる
        if (m_textString != text) {
            m_textString  = text;
            m_isTextDirty = true;
        }
    }
    void SetCenter(bool center) { m_isCentered = center; }

    // 描画処理のオーバーライド
    void OnRender(ID3D11DeviceContext *dc, float globalScale) override
    {
        // マネージャーからテキスト機能を取得
        if (!m_manager) return;
        TextManager *tm = m_manager->GetTextManager();
        if (!tm) return;

        // 文字が変わった時だけ、重い翻訳処理(メモリ確保)を走らせる
        if (m_isTextDirty) {
            m_cachedUTF32 = BitmapFont::UTF8toUTF32(m_textString);
            m_isTextDirty = false;
        }

        // ワールド座標取得 (1920x1080空間の座標)
        DirectX::XMFLOAT2 worldPos = GetWorldPosition();

        // テキストデータ構築
        // TextManagerはピクセル座標を受け取る仕様なので、ここでグローバルスケールを適用
        float drawX = worldPos.x * globalScale;
        float drawY = worldPos.y * globalScale;

        // フォントサイズも画面サイズに合わせてスケール
        float w = m_size.x * m_scale * globalScale;
        float h = m_size.y * m_scale * globalScale;

        // 翻訳済みの文字を、バッチ描画エンジンに直接投げる
        //tm->GetFontSprite()->Begin();
        tm->GetFontSprite()->AddStringUTF32(
            m_cachedUTF32, drawX, drawY, w, h, m_color, m_isCentered);
        //tm->GetFontSprite()->Flush(dc);
    }
};