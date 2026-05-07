#pragma once
#include "ECS/Common/CCL_Common.h"

// 生成直後のエンティティなど、まだSetParentできない場合に
// 「後でこの親を設定してね」と予約するためのコンポーネント
struct PendingParentComponent {
    CCL::ECS::EntityID parentID = CCL::ECS::InvalidEntityID;
};
