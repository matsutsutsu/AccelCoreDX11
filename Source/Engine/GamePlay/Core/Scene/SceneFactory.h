#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "Scene.h"

class SceneFactory {
public:
    using CreateFunc = std::function<Scene*()>;

    static SceneFactory& Instance() {
        static SceneFactory instance;
        return instance;
    }

    // シーン登録
    void Register(const std::string& name, CreateFunc func);

    // シーン生成
    Scene* Create(const std::string& name);

    // 登録済みシーン一覧を取得 (ImGuiでリスト表示に使える)
    std::vector<std::string> GetSceneNames() const;

private:
    SceneFactory() {}
    std::unordered_map<std::string, CreateFunc> registry;
};
