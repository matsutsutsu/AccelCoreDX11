#pragma once
#include "ComponentMeta.h"
#include <DirectXMath.h>
#include <json.hpp>
#include <type_traits> // メタプログラミング用

namespace _MetaJsonDX {
    inline void F2_to(nlohmann::json& j, const DirectX::XMFLOAT2& v)
    {
        j = nlohmann::json::array({ v.x, v.y });
    }
    inline void F2_from(const nlohmann::json& j, DirectX::XMFLOAT2& v)
    {
        if (j.is_array() && j.size() >= 2) {
            v.x = j[0];
            v.y = j[1];
        }
    }
    inline void F3_to(nlohmann::json& j, const DirectX::XMFLOAT3& v)
    {
        j = nlohmann::json::array({ v.x, v.y, v.z });
    }
    inline void F3_from(const nlohmann::json& j, DirectX::XMFLOAT3& v)
    {
        if (j.is_array() && j.size() >= 3) {
            v.x = j[0];
            v.y = j[1];
            v.z = j[2];
        }
    }
    inline void F4_to(nlohmann::json& j, const DirectX::XMFLOAT4& v)
    {
        j = nlohmann::json::array({ v.x, v.y, v.z, v.w });
    }
    inline void F4_from(const nlohmann::json& j, DirectX::XMFLOAT4& v)
    {
        if (j.is_array() && j.size() >= 4) {
            v.x = j[0];
            v.y = j[1];
            v.z = j[2];
            v.w = j[3];
        }
    }
} // namespace _MetaJsonDX

namespace ComponentMetaJson {
    using json = nlohmann::json;

    // ========================================================================
    // ★ 究極の綺麗な設計：コンポーネントごとの Trait（特性）
    // デフォルトでは全て false (自動シリアライズを使用する)
    // ========================================================================
    template <typename T>
    struct HasCustomSerialize : std::false_type {};

    // ------------------------------------------------------------------------
    // jsonName を使って保存・復元する基本関数
    // ------------------------------------------------------------------------
    inline void SerializeField(json& j, const FieldDescriptor& fd, const void* obj)
    {
        if (!fd.serialize || !fd.jsonName) return;

        switch (fd.kind) {
        case FieldKind::Float:   j[fd.jsonName] = RawField<const float>(obj, fd.offset); break;
        case FieldKind::Int:     j[fd.jsonName] = RawField<const int>(obj, fd.offset); break;
        case FieldKind::Bool:    j[fd.jsonName] = RawField<const bool>(obj, fd.offset); break;
        case FieldKind::UInt8:   j[fd.jsonName] = RawField<const uint8_t>(obj, fd.offset); break;
        case FieldKind::UInt16:  j[fd.jsonName] = RawField<const uint16_t>(obj, fd.offset); break;
        case FieldKind::UInt32:  j[fd.jsonName] = RawField<const uint32_t>(obj, fd.offset); break;
        case FieldKind::EntityID: j[fd.jsonName] = RawField<const uint64_t>(obj, fd.offset); break;
        case FieldKind::EnumU8:  j[fd.jsonName] = RawField<const uint8_t>(obj, fd.offset); break;
        case FieldKind::EnumInt: j[fd.jsonName] = RawField<const int>(obj, fd.offset); break;
        case FieldKind::Float2:  _MetaJsonDX::F2_to(j[fd.jsonName], RawField<const DirectX::XMFLOAT2>(obj, fd.offset)); break;
        case FieldKind::Float3:  _MetaJsonDX::F3_to(j[fd.jsonName], RawField<const DirectX::XMFLOAT3>(obj, fd.offset)); break;
        case FieldKind::Float4:  _MetaJsonDX::F4_to(j[fd.jsonName], RawField<const DirectX::XMFLOAT4>(obj, fd.offset)); break;
        case FieldKind::String:  j[fd.jsonName] = RawField<const std::string>(obj, fd.offset); break;
        }
    }

    inline void DeserializeField(const json& j, const FieldDescriptor& fd, void* obj)
    {
        if (!fd.serialize || !fd.jsonName || !j.contains(fd.jsonName)) return;

        switch (fd.kind) {
        case FieldKind::Float:   j[fd.jsonName].get_to(RawField<float>(obj, fd.offset)); break;
        case FieldKind::Int:     j[fd.jsonName].get_to(RawField<int>(obj, fd.offset)); break;
        case FieldKind::Bool:    j[fd.jsonName].get_to(RawField<bool>(obj, fd.offset)); break;
        case FieldKind::UInt8:   j[fd.jsonName].get_to(RawField<uint8_t>(obj, fd.offset)); break;
        case FieldKind::UInt16:  j[fd.jsonName].get_to(RawField<uint16_t>(obj, fd.offset)); break;
        case FieldKind::UInt32:  j[fd.jsonName].get_to(RawField<uint32_t>(obj, fd.offset)); break;
        case FieldKind::EntityID: j[fd.jsonName].get_to(RawField<uint64_t>(obj, fd.offset)); break;
        case FieldKind::EnumU8:  j[fd.jsonName].get_to(RawField<uint8_t>(obj, fd.offset)); break;
        case FieldKind::EnumInt: j[fd.jsonName].get_to(RawField<int>(obj, fd.offset)); break;
        case FieldKind::Float2:  _MetaJsonDX::F2_from(j[fd.jsonName], RawField<DirectX::XMFLOAT2>(obj, fd.offset)); break;
        case FieldKind::Float3:  _MetaJsonDX::F3_from(j[fd.jsonName], RawField<DirectX::XMFLOAT3>(obj, fd.offset)); break;
        case FieldKind::Float4:  _MetaJsonDX::F4_from(j[fd.jsonName], RawField<DirectX::XMFLOAT4>(obj, fd.offset)); break;
        case FieldKind::String:  j[fd.jsonName].get_to(RawField<std::string>(obj, fd.offset)); break;
        }
    }


    // ------------------------------------------------------------------------
    // 分岐を備えたパブリックAPI
    // ------------------------------------------------------------------------
    template <typename T> void Serialize(json& j, const T& component)
    {
        // ★ SFINAEの代わりに、Traitのブール値を直接見る
        if constexpr (HasCustomSerialize<T>::value) {
            ComponentMeta<T>::serialize(j, &component);
        }
        else {
            const void* obj = &component;
            for (const auto& fd : ComponentMeta<T>::Fields()) {
                SerializeField(j, fd, obj);
            }
        }
    }

    template <typename T> void Deserialize(const json& j, T& component)
    {
        void* obj = &component;
        for (const auto& fd : ComponentMeta<T>::Fields()) {
            DeserializeField(j, fd, obj);
        }
    }

    template <typename T, typename RegistryType>
    void RegisterMetaT(RegistryType& registry, const std::string& name)
    {
        typename RegistryType::MetaInfo info;
        info.name = name;

        info.serialize = [](json& j, const void* ptr) {
            Serialize(j, *static_cast<const T*>(ptr));
            };

        info.deserializeToWorld = [](auto* world, auto entity, const json& j) {
            // ★ ここもTraitのブール値で100%確実に分岐する
            if constexpr (HasCustomSerialize<T>::value) {
                ComponentMeta<T>::deserializeToWorld(world, entity, j);
            }
            else {
                T temp{};
                Deserialize(j, temp);
                world->template AddComponent<T>(entity, std::move(temp));
            }
            };

        registry.RegisterInternal(CCL::ECS::TypeInfo<T>::ID(), name, std::move(info));
    }
} // namespace ComponentMetaJson