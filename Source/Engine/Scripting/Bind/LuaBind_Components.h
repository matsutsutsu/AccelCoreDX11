#pragma once
#include <sol/sol.hpp>

// ‘O•ûéŒ¾
namespace CCL::ECS::Core {
    class World;
}

class LuaBind_Components {
  public:
    static void Bind(sol::state &lua, CCL::ECS::Core::World *world);
};