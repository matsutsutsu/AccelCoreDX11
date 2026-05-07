#include "LuaBind_Components.h"
#include "ECS/Common/CCL_Common.h"
#include "ECS/Core/CCL_World.h" // ComponentHandle と GetHandle のために必要

// 登録するコンポーネントのヘッダーをインクルード
#include "Game/Core/AllComponents.h"

using namespace DirectX;
using namespace CCL::ECS::Core;

// ==================================================================================
// ヘルパーマクロ: ハンドル経由のプロパティアクセスを定義
// ==================================================================================
// 役割:
// 1. ハンドルが有効(IsValid)かチェックする
// 2. 有効ならそのメンバ変数への参照を返す (Luaが読み書きできる)
// 3. 無効ならランタイムエラーを投げる (Lua側で pcall 等でキャッチ可能、または即停止)
#define HANDLE_PROP(CompType, MemberName)                                                          \
    sol::property(                                                                                 \
        [](ComponentHandle<CompType> &h) -> auto & {                                               \
            if (!h.IsValid())                                                                      \
                throw std::runtime_error(                                                          \
                    "Accessing destroyed component: " #CompType "::" #MemberName);                 \
            return h->MemberName;                                                                  \
        },                                                                                         \
        [](ComponentHandle<CompType> &h, const decltype(CompType::MemberName) &v) {                \
            if (h.IsValid()) {                                                                     \
                h->MemberName = v;                                                                 \
            }                                                                                      \
            else { /* 無効な場合は書き込みを無視、あるいはログ出力 */                              \
            }                                                                                      \
        })

void LuaBind_Components::Bind(sol::state &lua, CCL::ECS::Core::World *world)
{
    // --------------------------------------------------------
    // 1. 基本型 (Vector3など) - 変更なし
    // --------------------------------------------------------
    lua.new_usertype<XMFLOAT3>("Vector3",
        sol::constructors<XMFLOAT3(), XMFLOAT3(float, float, float)>(),
        "x",
        &XMFLOAT3::x,
        "y",
        &XMFLOAT3::y,
        "z",
        &XMFLOAT3::z);

    // --------------------------------------------------------
    // 2. コンポーネントの登録 (すべてハンドル型に変更)
    // --------------------------------------------------------

    // --- TransformComponent (Handle) ---
    lua.new_usertype<ComponentHandle<TransformComponent>>("Transform",
        "IsValid",
        &ComponentHandle<TransformComponent>::IsValid, // 有効確認用

        // 各メンバへのアクセス (マクロで生存チェック付き)
        "position",
        HANDLE_PROP(TransformComponent, position),
        "scale",
        HANDLE_PROP(TransformComponent, scale),
        "rotation",
        HANDLE_PROP(TransformComponent, rotation),
        "parentID",
        HANDLE_PROP(TransformComponent, parentID));

    // --- RigidBodyComponent (Handle) ---
    
    // --- NameComponent (Handle) ---
    // char配列の扱いは特殊なのでマクロを使わず手書き
    lua.new_usertype<ComponentHandle<NameComponent>>("NameInfo",
        "IsValid",
        &ComponentHandle<NameComponent>::IsValid,

        "name",
        sol::property(
            [](ComponentHandle<NameComponent> &h) -> std::string {
                if (!h.IsValid()) return "";
                return std::string(h->name);
            },
            [](ComponentHandle<NameComponent> &h, const std::string &s) {
                if (h.IsValid()) {
                    strncpy_s(h->name, s.c_str(), sizeof(h->name) - 1);
                }
            }));

    // --------------------------------------------------------
    // 3. コンポーネント取得関数の登録 (ハンドルを返すように変更)
    // --------------------------------------------------------
    sol::table entityTable = lua["Entity"].get_or_create<sol::table>();

    // Entity.GetTransform(id) -> ComponentHandle<TransformComponent> を返す
    entityTable.set_function("GetTransform",
        [world](CCL::ECS::EntityID id) { return world->GetHandle<TransformComponent>(id); });


    entityTable.set_function(
        "GetName", [world](CCL::ECS::EntityID id) { return world->GetHandle<NameComponent>(id); });

  
}