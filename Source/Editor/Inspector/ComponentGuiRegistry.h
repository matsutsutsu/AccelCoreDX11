#pragma once
#include <functional>
#include <unordered_map>
#include <string>
#include <typeindex>
#include <vector>
#include "ECS/Common/CCL_Common.h"
#include "ECS/Core/CCL_World.h"

using namespace CCL::ECS;

// コンポーネントごとのGUI描画ロジックと表示名を管理する
class ComponentGuiRegistry {
public:
    // シングルトンアクセス
    // 中身を消し、宣言のみにする
    static ComponentGuiRegistry &Instance();


    // EntityID と World* を受け取るように変更
    using GuiFunc = std::function<void(void*, EntityID, CCL::ECS::Core::World*)>;
    using CtorFunc = std::function<void(void*)>; // 追加：初期化用

    // 名前と関数をセットで保持する構造体
    struct Entry {
        std::string name;
        GuiFunc guiFunc = nullptr;
        CtorFunc ctor = nullptr;
        size_t size = 0;  
        // これがないと、Inspectorから追加したコンポーネントがメモリリークします
        Destructor dtor;

        TypeData::ConstructFunc constructor = nullptr;
        TypeData::AssignFunc    assigner    = nullptr;
        TypeData::MoveFunc      mover       = nullptr;

        // WorldとEntityIDだけで描画を完結させるためのラッパー関数
        // InspectorWindowはこれを使うことで、void* へのキャストやGetComponentを自分でしなくて済む
        std::function<void(CCL::ECS::Core::World *, EntityID)> drawInspector = nullptr;
    };


    // コンストラクタで初期化を呼ぶようにすると使い勝手が良い
    ComponentGuiRegistry() = default;


    // T: 型, name: 表示名, func: 描画関数
    template <typename T> void Register(const std::string &name, GuiFunc func)
    {
        Entry entry;
        entry.name    = name;
        entry.guiFunc = func;
        entry.ctor    = [](void *ptr) { new (ptr) T(); };
        entry.size    = sizeof(T);

        // TypeDataを作って全情報を取得
        TypeData td;
        td.Create<T>();

        entry.dtor        = td.destructor;
        entry.constructor = td.constructor;
        entry.assigner    = td.assigner;
        entry.mover       = td.mover;

        // ★ここで「安全な描画関数」を生成して保存する
        // 実行時に T型 の GetComponent を行い、ポインタが有効な場合のみ guiFunc を呼ぶ
        entry.drawInspector = [func](CCL::ECS::Core::World *world, EntityID entity) {
            // World経由で型安全に取得
            T *component = world->GetComponent<T>(entity);
            if (component) {
                // 生ポインタに変換して従来のGUI関数に渡す
                func(static_cast<void *>(component), entity, world);
            }
        };

        std::type_index key = typeid(T);

        // ★修正: operator[] は使わず insert で安全に登録（コンパイルエラー解決）
        _registry.insert({key, entry});

        // ★最強のハック: TypeID の取得を「実行時」まで遅延させる
        _pendingBindings.push_back([this, key]() {
            _typeIdToIndex.insert({CCL::ECS::TypeInfo<T>::ID(), key});
        });

    }

    // 描画前に必ず実行され、保留されていたID紐づけを一気に解決する
    void ResolveBindings() {
        if (_isResolved) return;
        for (auto& bindFunc : _pendingBindings) {
            bindFunc();
        }
        _pendingBindings.clear();
        _isResolved = true;
    }

    const Entry& GetEntry(TypeID id) {
        ResolveBindings(); // 取得前に解決
        return _registry.at(_typeIdToIndex.at(id));
    }

    std::string GetName(TypeID id) {
        ResolveBindings(); // 取得前に解決
        auto idIt = _typeIdToIndex.find(id);
        if (idIt == _typeIdToIndex.end())
            return "Unknown Component (ID: " + std::to_string(id) + ")";
        auto it = _registry.find(idIt->second);
        if (it != _registry.end()) return it->second.name;
        return "Unknown Component (ID: " + std::to_string(id) + ")";
    }

    void DrawInspector(TypeID id, CCL::ECS::Core::World* world, EntityID entity) {
        ResolveBindings(); // 描画前に解決
        auto idIt = _typeIdToIndex.find(id);
        if (idIt == _typeIdToIndex.end()) return;
        auto it = _registry.find(idIt->second);
        if (it != _registry.end() && it->second.drawInspector) {
            it->second.drawInspector(world, entity);
        }
    }

    std::vector<TypeID> GetAllTypeIDs() {
        ResolveBindings(); // 取得前に解決
        std::vector<TypeID> ids;
        ids.reserve(_typeIdToIndex.size());
        for (const auto& [id, _] : _typeIdToIndex) {
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end(), [this](TypeID a, TypeID b) {
            auto ia = _typeIdToIndex.at(a);
            auto ib = _typeIdToIndex.at(b);
            return _registry.at(ia).name < _registry.at(ib).name;
            });
        return ids;
    }


    // =========================================================
    //  TypeID からコンポーネント名を取得する逆引き関数
    // =========================================================
    const std::string& GetComponentName(TypeID id) {
        ResolveBindings(); // 取得前に未解決のバインディングを処理

        auto idIt = _typeIdToIndex.find(id);
        if (idIt != _typeIdToIndex.end()) {
            auto regIt = _registry.find(idIt->second);
            if (regIt != _registry.end()) {
                return regIt->second.name; // 既に保存されている名前を返す！
            }
        }

        // 見つからなかった場合用の空文字
        static const std::string unknown = "";
        return unknown;
    }

    // 描画実行
    void Draw(TypeID id, void* data, EntityID entity, CCL::ECS::Core::World* world);

private:
    // ★ メインストレージ: type_index → Entry（型に紐づく不変キー）
    std::unordered_map<std::type_index, Entry>   _registry;
    // ★ 逆引きマップ: TypeID → type_index（実行時に確定するIDから引く用）
    std::unordered_map<TypeID, std::type_index>  _typeIdToIndex;

    // ★遅延評価（Lazy Binding）用の変数群
    std::vector<std::function<void()>> _pendingBindings;
    bool _isResolved = false;

};