#pragma once
#include "Scene.h"
#include <string>

// World クラスが存在することをコンパイラに教える
namespace CCL::ECS::Core { class World; }

// シーンマネージャー（シングルトン）
class SceneManager {
private:
    SceneManager() {}
    ~SceneManager() {}

public:
    static SceneManager& Instance() {
        static SceneManager instance;
        return instance;
    }

    void Initialize();

    // 更新処理（シーン切り替え時のInitializeに必要なため、システム群を渡す）
    void FixedUpdate(float fixedTime);
    void Update(float elapsedTime, int frameIndex, DX12System* dx12System, SystemDataContext* systemDataContext, ResourceManager* resourceManager);

    // 描画処理（DX12のコマンドリストを渡す）
    void Render(DX12System* dx12System, CommandList* commandList, int frameIndex);

    void Clear();
    void ChangeScene(const std::string& name);
    void ChangeScene(Scene* scene);

    const std::string& GetCurrentSceneName() const { return _currentSceneName; }
    void SetCurrentSceneName(const std::string& name) { _currentSceneName = name; }

    // --- ECSデータ駆動のシーン保存・読み込み ---
    void SaveScene(CCL::ECS::Core::World* world, const std::string& filepath);
    void LoadScene(CCL::ECS::Core::World* world, const std::string& filepath);

private:
    Scene* currentScene = nullptr;
    Scene* nextScene = nullptr;
    std::string _currentSceneName = "";
};