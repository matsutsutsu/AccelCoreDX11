#pragma once

#include "Scene.h"
#include <string>

// World クラスが存在することをコンパイラに教える
namespace CCL::ECS::Core { class World; }

// シーンマネージャー
class SceneManager {
private:
    SceneManager() {}
    ~SceneManager() {}

public:
    // 唯一のインスタンス取得
    static SceneManager& Instance() {
        static SceneManager instance;
        return instance;
    }

    // 初期化
    void Initialize();

    // 終了化
    //void Finalize();

    // --- 追加：固定更新 ---
    void FixedUpdate(float fixedTime);

    // 更新処理
    void Update(float elapsedTime);

    // 描画処理
    void Render();

    // GUI描画
    void DrawGUI();

    // シーンクリア
    void Clear();

    // シーン切り替え
    void ChangeScene(const std::string& name);

    // インスタンスを直接渡すオーバーロード
    void ChangeScene(Scene* scene);

    // 現在のシーン名を取得・設定
    const std::string& GetCurrentSceneName() const { return _currentSceneName; }
    void SetCurrentSceneName(const std::string& name) { _currentSceneName = name; }

    // --- ECSデータ駆動のシーン保存・読み込み ---
    void SaveScene(CCL::ECS::Core::World* world, const std::string& filepath);
    void LoadScene(CCL::ECS::Core::World* world, const std::string& filepath);

private:


    Scene* currentScene = nullptr; // 現在のシーン
    Scene* nextScene = nullptr;    // 次に遷移するシーン

    // 現在のシーン名を保持する変数
    std::string _currentSceneName = "";
};