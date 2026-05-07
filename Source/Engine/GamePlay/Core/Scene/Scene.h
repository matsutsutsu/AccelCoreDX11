#pragma once

#include <memory>


// シーン基底
class Scene
{
  protected:
    // 状態管理は一番親のクラスが責任を持つ
    bool _isReady = false;

public:
    Scene() {}
    virtual ~Scene() {}

    // 初期化が完了しているかどうかのフラグ
    // LoadingSceneで裏読みした場合は true を返すようにする
    bool IsReady() const { return _isReady; }

    // マネージャーから「初期化完了」を通知するための関数
    void SetReady() { _isReady = true; }

    // 初期化
    virtual void Initialize() = 0;

    // GPUセットアップなど、Initialize後にメインスレッドで1回だけ呼ばれる処理（必要に応じてオーバーライド）
    virtual void Start() {} 

    // 終了化
    virtual void Finalize() = 0;

    // --- 固定タイムステップ更新（物理・ロジック用） ---
    // デフォルトでは何もしない（必要に応じてオーバーライド）
    virtual void FixedUpdate(float fixedTime) {}

    // 更新処理
    virtual void Update(float elapsedTime) = 0;

    // 描画処理
    virtual void Render() = 0;

    // GUI描画
    virtual void DrawGUI() = 0;


};