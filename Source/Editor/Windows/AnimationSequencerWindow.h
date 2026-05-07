#pragma once
#include "Editor/Core/EditorWindow.h"
#include <memory>

// 前方宣言: 実装クラスをcppに隠蔽するため、ここでは「クラスがあるよ」とだけ伝えます
// これにより、このヘッダーを読み込む他のファイルが ImSequencer.h に依存しなくなります
class AnimationSequenceImpl;
struct SequencerCurveDelegate;

class AnimationSequencerWindow : public EditorWindow {
  public:
    // コンストラクタ（初期化）とデストラクタ（後片付け）
    AnimationSequencerWindow();
    virtual ~AnimationSequencerWindow();

  protected:
    // ウィンドウの中身を描画する関数（EditorWindowからの継承）
    void DrawContents(EditorContext &context) override;

  private:
    // 実装部分（データ管理クラス）へのポインタ
    // unique_ptrを使うことで、メモリ管理を自動化しています
    std::unique_ptr<AnimationSequenceImpl> _sequencerImpl;

    // カーブ管理デリゲート
    std::unique_ptr<SequencerCurveDelegate> _curveDelegate;

    // シーケンサーの表示状態管理用の変数
    bool _expanded;
    int  _selectedEntry;
    int  _firstFrame;

    // カーブエディタの表示を自動調整するためのフラグ
    bool _fitCurveView = true;
};