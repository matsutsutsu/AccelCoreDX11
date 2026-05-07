#pragma once
#include <unordered_map>
#include "ECS/Common/CCL_Common.h"

class SerializationContext {
public:
    // マップへのアクセス
    // 修正: Key (昔のID) を uint64_t にする
    // EntityID型だと32bitの場合に溢れる可能性があるため
    static std::unordered_map<uint64_t, CCL::ECS::EntityID>& GetIdMap() {
        static std::unordered_map<uint64_t, CCL::ECS::EntityID> map;
        return map;
    }

    // ロード開始時に必ず呼ぶ
    static void Clear() {
        GetIdMap().clear();
    }

    // マッピングを登録 (旧ID -> 新ID)
    static void RegisterID(uint64_t oldID, CCL::ECS::EntityID newID) {
        GetIdMap()[oldID] = newID;
    }

    // 古いIDを新しいIDに変換して返す
    static CCL::ECS::EntityID ResolveID(uint64_t oldID) {
        if (oldID == 0) return 0;

        auto& map = GetIdMap();
        if (map.find(oldID) != map.end()) {
            return map[oldID];
        }
        return 0;
    }
};