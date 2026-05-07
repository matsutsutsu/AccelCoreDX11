#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

enum class FieldKind : uint8_t {
    Float,
    Int,
    Bool,
    UInt8,
    UInt16, // 16ビット符号なし整数
    UInt32, // 32ビット符号なし整数
    EntityID,
    EnumU8,
    EnumInt,
    Float2,
    Float3,
    Float4,
    String
};

struct FieldDescriptor {
    const char        *name      = nullptr; // ★UIでの表示名 (DisplayName)
    const char        *jsonName  = nullptr; // ★JSONでの保存キー (JsonName) 追加
    FieldKind          kind      = FieldKind::Float;
    size_t             offset    = 0;
    float              dragSpeed = 0.05f;
    float              rangeMin  = 0.0f;
    float              rangeMax  = 0.0f;
    const char        *category  = nullptr;
    const char *const *enumNames = nullptr;
    int                enumCount = 0;
    bool               serialize = true;
};

// --- インライン生成関数 ---

// TODO: なんかUint8から文法がバラバラだから後で統一する
inline FieldDescriptor FD_Float(const char *name, const char *jsonName, size_t offset, float speed = 0.05f, float rangeMin = 0.0f, float rangeMax = 0.0f, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::Float; d.offset = offset; d.dragSpeed = speed; d.rangeMin = rangeMin; d.rangeMax = rangeMax; d.category = category; return d;
}
inline FieldDescriptor FD_Int(const char *name, const char *jsonName, size_t offset, float rangeMin = 0.0f, float rangeMax = 0.0f, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::Int; d.offset = offset; d.rangeMin = rangeMin; d.rangeMax = rangeMax; d.category = category; return d;
}
inline FieldDescriptor FD_Bool(const char *name, const char *jsonName, size_t offset, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::Bool; d.offset = offset; d.category = category; return d;
}
inline FieldDescriptor FD_UInt8(const char* name, const char* jsonName, size_t offset, float speed = 1.0f, float rangeMin = 0.0f, float rangeMax = 0.0f, const char* category = nullptr, bool serialize = true) {
    return { name, jsonName, FieldKind::UInt8, offset, speed, rangeMin, rangeMax, category, nullptr, 0, serialize };
}

inline FieldDescriptor FD_UInt16(const char* name, const char* jsonName, size_t offset, float speed = 1.0f, float rangeMin = 0.0f, float rangeMax = 0.0f, const char* category = nullptr, bool serialize = true) {
    return { name, jsonName, FieldKind::UInt16, offset, speed, rangeMin, rangeMax, category, nullptr, 0, serialize };
}

inline FieldDescriptor FD_UInt32(const char* name, const char* jsonName, size_t offset, float speed = 1.0f, float rangeMin = 0.0f, float rangeMax = 0.0f, const char* category = nullptr, bool serialize = true) {
    return { name, jsonName, FieldKind::UInt32, offset, speed, rangeMin, rangeMax, category, nullptr, 0, serialize };
}

inline FieldDescriptor FD_EntityID(const char* name, const char* jsonName, size_t offset, const char* category = nullptr, bool serialize = true) {
    return { name, jsonName, FieldKind::EntityID, offset, 0.0f, 0.0f, 0.0f, category, nullptr, 0, serialize };
}
inline FieldDescriptor FD_EnumU8(const char *name, const char *jsonName, size_t offset, const char *const *enumNames, int enumCount, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::EnumU8; d.offset = offset; d.enumNames = enumNames; d.enumCount = enumCount; d.category = category; return d;
}
inline FieldDescriptor FD_EnumInt(const char *name, const char *jsonName, size_t offset, const char *const *enumNames, int enumCount, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::EnumInt; d.offset = offset; d.enumNames = enumNames; d.enumCount = enumCount; d.category = category; return d;
}
inline FieldDescriptor FD_Float2(const char *name, const char *jsonName, size_t offset, float speed = 0.05f, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::Float2; d.offset = offset; d.dragSpeed = speed; d.category = category; return d;
}
inline FieldDescriptor FD_Float3(const char *name, const char *jsonName, size_t offset, float speed = 0.05f, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::Float3; d.offset = offset; d.dragSpeed = speed; d.category = category; return d;
}
inline FieldDescriptor FD_Float4(const char *name, const char *jsonName, size_t offset, float speed = 0.05f, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::Float4; d.offset = offset; d.dragSpeed = speed; d.category = category; return d;
}
inline FieldDescriptor FD_String(const char *name, const char *jsonName, size_t offset, const char *category = nullptr) {
    FieldDescriptor d{}; d.name = name; d.jsonName = jsonName; d.kind = FieldKind::String; d.offset = offset; d.category = category; return d;
}


// ============================================================================
//  メタ宣言用 共通マクロ群
// 全コンポーネントの .cpp から共通して呼び出されるフォーマット。
// offsetof を内部に隠蔽し、記述ミスをコンパイル時に検出します。
// ============================================================================

#define META_FIELD_FLOAT(ClassType, FieldName, JsonName, DisplayName, Speed, Min, Max, Category) \
    FD_Float(DisplayName, JsonName, offsetof(ClassType, FieldName), Speed, Min, Max, Category)

#define META_FIELD_INT(ClassType, FieldName, JsonName, DisplayName, Min, Max, Category) \
    FD_Int(DisplayName, JsonName, offsetof(ClassType, FieldName), Min, Max, Category)

#define META_FIELD_BOOL(ClassType, FieldName, JsonName, DisplayName, Category) \
    FD_Bool(DisplayName, JsonName, offsetof(ClassType, FieldName), Category)

#define META_FIELD_UINT8(Comp, Field, Name, JsonName, Speed, Category) \
    FD_UInt8(Name, JsonName, offsetof(Comp, Field), Speed, 0.0f, 0.0f, Category)

#define META_FIELD_UINT16(Comp, Field, Name, JsonName, Speed, Category) \
    FD_UInt16(Name, JsonName, offsetof(Comp, Field), Speed, 0.0f, 0.0f, Category)

#define META_FIELD_UINT32(Comp, Field, Name, JsonName, Speed, Category) \
    FD_UInt32(Name, JsonName, offsetof(Comp, Field), Speed, 0.0f, 0.0f, Category)

#define META_FIELD_ENTITY_ID(Comp, Field, Name, JsonName, Category) \
    FD_EntityID(Name, JsonName, offsetof(Comp, Field), Category)

#define META_FIELD_ENUM_U8(ClassType, FieldName, JsonName, DisplayName, EnumNames, EnumCount, Category) \
    FD_EnumU8(DisplayName, JsonName, offsetof(ClassType, FieldName), EnumNames, EnumCount, Category)

#define META_FIELD_ENUM_INT(ClassType, FieldName, JsonName, DisplayName, EnumNames, EnumCount, Category) \
    FD_EnumInt(DisplayName, JsonName, offsetof(ClassType, FieldName), EnumNames, EnumCount, Category)

#define META_FIELD_FLOAT2(ClassType, FieldName, JsonName, DisplayName, Speed, Category) \
    FD_Float2(DisplayName, JsonName, offsetof(ClassType, FieldName), Speed, Category)

#define META_FIELD_FLOAT3(ClassType, FieldName, JsonName, DisplayName, Speed, Category) \
    FD_Float3(DisplayName, JsonName, offsetof(ClassType, FieldName), Speed, Category)

#define META_FIELD_FLOAT4(ClassType, FieldName, JsonName, DisplayName, Speed, Category) \
    FD_Float4(DisplayName, JsonName, offsetof(ClassType, FieldName), Speed, Category)

#define META_FIELD_STRING(ClassType, FieldName, JsonName, DisplayName, Category) \
    FD_String(DisplayName, JsonName, offsetof(ClassType, FieldName), Category)


// ============================================================================
//  2. コンポーネントメタデータ テンプレート基底
// ============================================================================
template <typename T> struct ComponentMeta {
    static constexpr bool        registered  = false;
    static constexpr const char *displayName = "(unregistered)";

    // 【カスタムGUIフック】
    // 手動でImGuiのコードを書きたい場合、特殊化する側でこれを true にします。
    static constexpr bool hasCustomGui = false;

    // デフォルトはすべて保存対象（保存したくない場合は特殊化側で false にする）
    static constexpr bool        isSerializable = true;

    // ★ 究極の綺麗な設計: カスタムシリアライズを行うかどうかの明示的フラグ
    static constexpr bool hasCustomSerialize = false;

    // カスタムGUIの実装関数 (hasCustomGui = true のときのみ描画システムから呼ばれる)
    // 戻り値: 値が変更されたら true を返す (Undo/Redoシステムのフラグ用)
    // 外部のエンティティやシステム（World等）にアクセスできるようコンテキストを受け取る
    static bool CustomGui(T & /*comp*/, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr) { return false; }

    static const std::vector<FieldDescriptor> &Fields()
    {
        static const std::vector<FieldDescriptor> empty;
        return empty;
    }
};


// コンポーネントの先頭アドレス（void* base）と、ズレ（size_t offset）を受け取る
template <typename FieldT> inline FieldT &RawField(void *base, size_t offset)
{
    // ① static_cast<char *>(base)
    // 「void*」は単なる「場所」なので、何歩進めばいいかわかりません。
    // 「char*」に変換することで、「1歩 ＝ 1バイトずつ進める状態」にします。

    // ② + offset
    // 先頭アドレスから、指定されたバイト数（歩数）だけ進みます。
    // 例：attackRangeなら「4」なので、1000番地 + 4 = 1004番地 に到着します。

    // ③ reinterpret_cast<FieldT *>( ... )
    // 1004番地に到着しましたが、コンピュータはそこに何が入っているか知りません。
    // なので、「ここにある4バイトは float（FieldT）として読み取ってね！」と強制的に教えます。
    return *reinterpret_cast<FieldT *>(static_cast<char *>(base) + offset);
}
template <typename FieldT> inline const FieldT &RawField(const void *base, size_t offset)
{
    return *reinterpret_cast<const FieldT *>(static_cast<const char *>(base) + offset);
}