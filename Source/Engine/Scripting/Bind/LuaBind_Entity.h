#pragma once

#include <sol/sol.hpp>

// Worldクラスの前方宣言
namespace CCL::ECS::Core {
    class World;
}

class LuaBind_Entity {
  public:
    // この関数で Entity テーブルに関数を登録する
    static void Bind(sol::state &lua, CCL::ECS::Core::World *world);
};