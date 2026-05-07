#pragma once
#include "Game/Scenes/BaseScene.h"

#include <atomic>
#include <future>
#include <thread>



class SceneLoading : public BaseScene {
  private:
    // 次に遷移するシーン
    Scene *_nextScene = nullptr;

    // 非同期処理用
    std::thread      *_thread     = nullptr;
    std::atomic<bool> _isFinished = false;

    // UI (ローディング中のアニメーション用)

  public:
    // コンストラクタで次のシーンを受け取る
    SceneLoading(Scene *nextScene);
    virtual ~SceneLoading() override;

    void Initialize() override;
    void Finalize() override;
    void Update(float elapsedTime) override;

    // 描画はBaseSceneのRender -> OnRenderUIの流れを使う
    void OnRenderUI() override;
    void OnDrawImGui() override;

    float time = 0.0f;
  private:
    // スレッドで実行する関数
    void LoadingThread();
};