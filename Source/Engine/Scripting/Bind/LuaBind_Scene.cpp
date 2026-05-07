#include "ECS/Core/CCL_World.h"
#include "LuaBind_Scene.h"
#include "Engine/GamePlay/Core/Scene/SceneManager.h"

// 必要なシーンやローダーをインクルード
#include "Game/Scenes/SceneLoading.h"


void LuaBind_Scene::Bind(sol::state &lua, CCL::ECS::Core::World *world)
{
    auto &sm = SceneManager::Instance();

    // ---------------------------------------------------
    // 1. Sceneテーブル (シーン管理)
    // ---------------------------------------------------
    auto scene = lua.create_named_table("Scene");

    // 現在のシーン名を取得
    scene.set_function("GetName", [&sm]() -> std::string { return sm.GetCurrentSceneName(); });

    // シーン遷移 (文字列からクラスを生成してロード)
    scene.set_function("Load", [&sm](std::string nextName) {
        BaseScene *target = nullptr;

        //if (nextName == "Game") {
            //target = new GameScene();
        //}
        //else if (nextName == "Shop") {
        //    target = new ShopScene();
        //}

        if (target) {
            // 名前更新
            sm.SetCurrentSceneName(nextName);
            // ロード画面を挟んで遷移
            sm.ChangeScene(new SceneLoading(target));
            printf("[Lua] Scene Changing to: %s\n", nextName.c_str());
        }
    });

  
    // ---------------------------------------------------
    // 3. Gameテーブル (全体システム)
    // ---------------------------------------------------

}