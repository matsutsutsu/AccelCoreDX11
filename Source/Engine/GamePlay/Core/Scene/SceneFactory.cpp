#include "SceneFactory.h"

void SceneFactory::Register(const std::string& name, CreateFunc func)
{
	registry[name] = func;
}

Scene* SceneFactory::Create(const std::string& name)
{
    auto it = registry.find(name);
    if (it != registry.end()) {
        return it->second(); // 登録された関数を実行 → Scene* を返す
    }
    return nullptr;
}

std::vector<std::string> SceneFactory::GetSceneNames() const
{
    std::vector<std::string> names;
    for (auto& kv : registry) {
        names.push_back(kv.first);
    }
    return names;
}
