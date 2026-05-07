#include "LuaBind_Entity.h"
#include "ECS/Common/CCL_Common.h" // š’Ç‰Á
#include "ECS/Core/CCL_World.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

void LuaBind_Entity::Bind(sol::state &lua, CCL::ECS::Core::World *world)
{
    auto table = lua.create_named_table("Entity");

    // šC³: int id -> CCL::ECS::EntityID id
    table.set_function("SetPos", [world](CCL::ECS::EntityID id, float x, float y, float z) {
        if (auto *t = world->GetComponent<TransformComponent>(id)) {
            t->position = {x, y, z};
        }
    });

    // šC³: int id -> CCL::ECS::EntityID id
    table.set_function("GetPos", [world](CCL::ECS::EntityID id) -> std::tuple<float, float, float> {
        if (auto *t = world->GetComponent<TransformComponent>(id)) {
            return std::make_tuple(t->position.x, t->position.y, t->position.z);
        }
        return std::make_tuple(0.0f, 0.0f, 0.0f);
    });
}