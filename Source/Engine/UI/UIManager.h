#pragma once
#include "Engine/Graphics/Renderer/Sprite.h"
#include "Engine/UI/Text/TextManager.h" // テキスト統合のため追加
#include <memory>
#include <unordered_map>
#include <vector>

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

// 前方宣言 (UIElement.h を include すると循環参照になるため)
class UIElement;
struct UIMouseState;
struct DragPayload;

class UIManager {
  private:
    // ルート要素のみを保持（子要素は親が持つため、ここにはルートだけ入れる）
    std::vector<std::shared_ptr<UIElement>> m_rootElements;

    // スプライトリソース
    std::unordered_map<std::string, std::unique_ptr<Sprite>> m_sprites;

    // テキストマネージャーへの参照
    TextManager *m_textManager = nullptr;

    ID3D11Device        *m_device  = nullptr;
    ID3D11DeviceContext *m_context = nullptr;

    // ドラッグ＆ドロップ用
    UIElement *m_pDragSource     = nullptr;
    UIElement *m_pHoveredElement = nullptr;

    // メンバ変数の初期化が必要です（ヘッダ内で簡易初期化、またはコンストラクタで）
    bool m_isDragging = false;
    // DragPayload m_currentPayload; //
    // payloadはcpp側で扱うか、includeが必要。今回はポインタ管理で回避も可能ですが、一旦既存維持のためcppでincludeします

  public:
    UIManager(ID3D11Device *device, ID3D11DeviceContext *context);
    ~UIManager();

    // --- テキスト管理 ---
    void         SetTextManager(TextManager *textManager) { m_textManager = textManager; }
    TextManager *GetTextManager() const { return m_textManager; }

    // --- 要素管理 ---
    // 親を持たないルート要素として追加する
    void AddElement(std::shared_ptr<UIElement> element);

    // 全削除
    void ClearElements();

    // --- リソース ---
    void    LoadSprite(const std::string &name, const char *filepath);
    Sprite *GetSprite(const std::string &name) const;

    // --- メインループ ---
    void Update(float dt, const UIMouseState &mouse);
    void Render();

    // --- ユーティリティ ---
    // 画面サイズに応じた拡大率を取得 (1920x1080基準)
    float GetGlobalScale() const;

    // デバッグ用
    void DrawDebugGUI();
};