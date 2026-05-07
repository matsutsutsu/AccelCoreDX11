#pragma once
#include <functional>
#include <unordered_map>
#include <string>
#include <type_traits>
#include "ECS/Core/CCL_World.h"
#include <json.hpp>
//#include "ComponentSerializers.h"

// 古い手動シリアライザの代わりに、強力な自動シリアライザを読み込む
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// ImGui(インスペクタ)の自動登録に必要なヘッダーをコアで読み込む
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"

#include <Windows.h>

// ==========================================================
// ★ SFINAE 1: ComponentMeta<T> に isSerializable があるか判定
// ==========================================================
template <typename T, typename = void>
struct HasIsSerializable : std::true_type {};

template <typename T>
struct HasIsSerializable<T, std::void_t<decltype(ComponentMeta<T>::isSerializable)>>
    : std::bool_constant<ComponentMeta<T>::isSerializable> {
};

// ==========================================================
// ★ SFINAE 2: ComponentMeta<T> に Remap 関数があるか判定
// ==========================================================
template <typename T, typename = void>
struct HasRemap : std::false_type {};

template <typename T>
struct HasRemap<T, std::void_t<decltype(ComponentMeta<T>::Remap(
    std::declval<T&>(),
    std::declval<const std::unordered_map<uint64_t, CCL::ECS::EntityID>&>()))>>
    : std::true_type {};

class ComponentRegistry {
public:
    // JSONへ書き出す関数
    using SerializeFunc = std::function<void(nlohmann::json&, const void*)>;
    // JSONからWorldへ復元する関数
    using DeserializeToWorldFunc = std::function<void(CCL::ECS::Core::World*, CCL::ECS::EntityID, const nlohmann::json&)>;

    // 復元された実体のIDを翻訳する関数 (キーは uint64_t に対応)
    using RemapFunc = std::function<void(CCL::ECS::Core::World*, CCL::ECS::EntityID, const std::unordered_map<uint64_t, CCL::ECS::EntityID>&)>;


    struct MetaInfo {
        std::string name;
        SerializeFunc serialize;
        DeserializeToWorldFunc deserializeToWorld;

        // ★IDリマップ用
        RemapFunc remap = nullptr;

        // セーブ用の動的チェックと抽出関数
        std::function<bool(CCL::ECS::Core::World*, CCL::ECS::EntityID)> hasComponent = nullptr;
        std::function<void(CCL::ECS::Core::World*, CCL::ECS::EntityID, nlohmann::json&)> saveToJSON = nullptr;
    };

    static ComponentRegistry &Instance();

    // 登録されているすべてのメタデータを取得する
    const std::unordered_map<std::string, MetaInfo>& GetAllMeta() const { return _nameToMeta; }

    // 型Tを登録する
    template <typename T>
    void Register(const std::string& name) {
        MetaInfo info;
        info.name = name;

        // 1. [Save(旧)用] void* を T* にキャストして to_json
        info.serialize = [](nlohmann::json& j, const void* ptr) {
            const T* component = static_cast<const T*>(ptr);
            ComponentMetaJson::Serialize(j, *component);
            };

        // 2. [Load用] JSONから一時変数Tを作成し、World にコピーして渡す
        info.deserializeToWorld = [](CCL::ECS::Core::World* world,
            CCL::ECS::EntityID entity,
            const nlohmann::json& j)
            {
                // ★ HasCustomSerialize を見て分岐する
                if constexpr (ComponentMetaJson::HasCustomSerialize<T>::value) {
                    // カスタム実装がある型はそちらに委譲
                    ComponentMeta<T>::deserializeToWorld(world, entity, j);
                }
                else {
                    T temp{};
                    ComponentMetaJson::Deserialize(j, temp);
                    world->AddComponent<T>(entity, std::move(temp));
                }
            };

        // 3. [Remap用] ★ if constexpr を使って Remap が存在するときだけ登録する
        if constexpr (HasRemap<T>::value) {
            info.remap = [](CCL::ECS::Core::World* world, CCL::ECS::EntityID entity, const std::unordered_map<uint64_t, CCL::ECS::EntityID>& idMap) {
                if (auto* comp = world->GetComponent<T>(entity)) {
                    ComponentMeta<T>::Remap(*comp, idMap);
                }
                };
        }
        else {
            info.remap = nullptr;
        }

        // 4. [Scene Save用] 保存除外判定 (isSerializable)
        // C++17の if constexpr を使い、isSerializable == true の型だけ抽出機能を登録する
        if constexpr (HasIsSerializable<T>::value) {
            info.hasComponent = [](CCL::ECS::Core::World* w, CCL::ECS::EntityID e) {
                return w->HasComponent<T>(e);
                };

            info.saveToJSON = [](CCL::ECS::Core::World* w, CCL::ECS::EntityID e, nlohmann::json& j) {
                if (auto* comp = w->GetComponent<T>(e)) {
                    nlohmann::json compJson;
                    ComponentMetaJson::Serialize(compJson, *comp);
                    j[ComponentMeta<T>::displayName] = compJson;
                }
                };
        }

        // マップ登録
        _nameToMeta[name] = info;

        // TypeID -> Name
        auto typeId = CCL::ECS::TypeInfo<T>::ID();
        _typeIdToName[typeId] = name;
    }

    const MetaInfo* GetInfoByName(const std::string& name) {
        auto it = _nameToMeta.find(name);
        return (it != _nameToMeta.end()) ? &it->second : nullptr;
    }

    std::string GetNameByTypeID(CCL::ECS::TypeID id) {
        auto it = _typeIdToName.find(id);
        return (it != _typeIdToName.end()) ? it->second : "";
    }

    // 【重要】実装を登録するための非テンプレート関数
    void RegisterInternal(CCL::ECS::TypeID typeId, const std::string &name, MetaInfo &&info)
    {
        _nameToMeta[name]     = info;
        _typeIdToName[typeId] = name;
    }

private:
    std::unordered_map<std::string, MetaInfo> _nameToMeta;
    std::unordered_map<CCL::ECS::TypeID, std::string> _typeIdToName;
};


// ── リンカ強制保護 (究極版：COMDAT最適化の完全破壊) ────────────────────────
// 登録用変数だけが最適化で切り捨てられるのを防ぐため、
// エクスポート保護されたアンカー変数の中でラムダ式を回し、登録処理を道連れにする。
// ─────────────────────────────────────────────────────────────────────


// ── リンカ強制保護 (確実版) ────────────────────────────────────────────────
// __declspec(dllexport) は EXE ビルドで /OPT:REF に対して不安定。
// #pragma comment(linker, "/include:X") はリンカの参照グラフに
// 直接シンボルを追加する唯一の確実な方法。
// ─────────────────────────────────────────────────────────────────────────

#ifdef _MSC_VER
    // 2段マクロ展開: T が完全展開された後に文字列化する
#define _CCL_LINK_IMPL(sym) __pragma(comment(linker, "/include:" #sym))
#define _CCL_LINK(sym)      _CCL_LINK_IMPL(sym)
#define _CCL_USED
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: コンパイル段階で削除を阻止する
#define _CCL_LINK(sym)
#define _CCL_USED __attribute__((used))
#else
#define _CCL_LINK(sym)
#define _CCL_USED
#endif

#define REGISTER_COMPONENT(T, Name)                                                                     \
    namespace {                                                                                         \
        struct AutoReg_##T {                                                                            \
            AutoReg_##T() {                                                                             \
                /* ★詳細診断 */                                                                        \
                char buf[256];                                                                          \
                bool alreadyRegistered = (ComponentRegistry::Instance().GetInfoByName(Name) != nullptr);\
                sprintf_s(buf, "[CCL_DIAG] AutoReg<%s> called. already=%d\n", Name, alreadyRegistered); \
                OutputDebugStringA(buf);                                                                \
                if (!alreadyRegistered) {                                                               \
                    ComponentRegistry::Instance().Register<T>(Name);                                    \
                }                                                                                       \
                ComponentMetaImGui::RegisterGuiMeta<T>(ComponentGuiRegistry::Instance());               \
                OutputDebugStringA("[CCL_DIAG] RegisterGuiMeta done for " Name "\n");                   \
                                                                                                        \
                /* ★★ TypeID 確認ログを追加 */                                                          \
                char buf2[128];                                                                         \
                sprintf_s(buf2, "[CCL_DIAG_ID] %s -> TypeInfo::ID() = %llu\n",                          \
                    Name, (unsigned long long)CCL::ECS::TypeInfo<T>::ID());                             \
                OutputDebugStringA(buf2);                                                               \
            }                                                                                           \
        };                                                                                              \
    }                                                                                                   \
    /* ★修正点1: dllexport を廃止し、pragma で確実に参照を付与         */                              \
    /* ★修正点2: static ローカルを廃止し、初期化式に直接コンストラクタを呼ぶ */                         \
    /* (AutoReg_##T{}, 1) = 一時オブジェクトを構築→登録実行→即破棄、値は 1 */                        \
    extern "C" _CCL_USED int g_MetaAnchor_##T = (AutoReg_##T{}, 1);                                     \
    _CCL_LINK(g_MetaAnchor_##T)