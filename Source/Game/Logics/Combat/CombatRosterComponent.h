#pragma once
#include "ECS/Common/CCL_Common.h"



// 武器や部位のIDを即座に引くための辞書（固定長配列でPOD化）
struct CombatRosterComponent {
    struct Entry {
        char tag[32]; // "RightHand", "LeftFoot" など
        CCL::ECS::EntityID id;
    };
    int count = 0;
    Entry entries[4]; // ボスが持つ武器の最大数

    CCL::ECS::EntityID Find(const char* targetTag) const {
        for (int i = 0; i < count; ++i) {
            if (strcmp(entries[i].tag, targetTag) == 0) return entries[i].id;
        }
        return 0;
    }
};