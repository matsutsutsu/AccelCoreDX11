#pragma once
#include "ECS/Common/CCL_Common.h"

// 「自分は〇〇の車の物理球体である」と証明するためのタグコンポーネント
struct PlayerCarSphereTag {
    CCL::ECS::EntityID parentCarID = CCL::ECS::InvalidEntityID; // 見た目の車のID
};