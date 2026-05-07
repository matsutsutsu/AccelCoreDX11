#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// 隠れ場所コンポーネント
#include "HidingComponents.h"

// 名前空間の外で、マクロに渡すための「::を含まない別名」を定義
using HidingUnder = HidingSpotTag::Under;
using HidingOpen = HidingSpotTag::Open;
using HidingTopIn = HidingSpotTag::TopIn;


// ============================================================================
// Hiding Spot Component Meta
// ============================================================================
template <> struct ComponentMeta<HidingSpotComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Hiding Spot";
    static constexpr bool        hasCustomGui = true;

    // JSONシリアライズ対象のフィールド定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // 潜伏時の位置オフセット
            META_FIELD_FLOAT3(HidingSpotComponent, localOffset, "localOffset", "Local Offset", 0.05f, "Offsets"),
            // 脱出時の位置オフセット
            META_FIELD_FLOAT3(HidingSpotComponent, exitOffset, "exitOffset", "Exit Offset", 0.05f, "Offsets"),
        };
        return fields;
    }

    // インスペクター上での描画
    static bool CustomGui(HidingSpotComponent& comp, unsigned long long entityID = 0, void* worldPtr = nullptr)
    {
        bool changed = false;
        auto* world = static_cast<CCL::ECS::Core::World*>(worldPtr);

        // --- 1. オフセット設定の描画 ---
        ImGui::SeparatorText("Offset Settings");
        for (const auto& fd : Fields()) {
            if (ComponentMetaImGui::DrawField(fd, &comp)) {
                changed = true;
            }
        }

        // --- 2. 潜伏タイプ（Tag）の選択 ---
        ImGui::SeparatorText("Hiding Type Tag");

        // 現在どのタグがついているかを確認
        enum class HidingType { None, Under, Open, TopIn };
        HidingType currentType = HidingType::None;

        if (world->HasComponent<HidingSpotTag::Under>(entityID)) currentType = HidingType::Under;
        else if (world->HasComponent<HidingSpotTag::Open>(entityID)) currentType = HidingType::Open;
        else if (world->HasComponent<HidingSpotTag::TopIn>(entityID)) currentType = HidingType::TopIn;

        const char* typeNames[] = { "None", "Under (潜り込み)", "Open (ロッカー)", "TopIn (ドラム缶)" };
        int selectedIndex = static_cast<int>(currentType);

        if (ImGui::Combo("Spot Tag Type", &selectedIndex, typeNames, IM_ARRAYSIZE(typeNames))) {
            // 一旦すべてのタグを削除（排他的な選択を実現）
            world->RequestRemoveComponent<HidingSpotTag::Under>(entityID);
            world->RequestRemoveComponent<HidingSpotTag::Open>(entityID);
            world->RequestRemoveComponent<HidingSpotTag::TopIn>(entityID);

            // 選択された新しいタグを付与
            HidingType newType = static_cast<HidingType>(selectedIndex);
            if (newType == HidingType::Under) world->AddComponent<HidingSpotTag::Under>(entityID);
            else if (newType == HidingType::Open) world->AddComponent<HidingSpotTag::Open>(entityID);
            else if (newType == HidingType::TopIn) world->AddComponent<HidingSpotTag::TopIn>(entityID);

            changed = true;
        }

        // --- 3. 状態のデバッグ表示 ---
        ImGui::SeparatorText("Runtime Status");

        // 誰かが入っているかどうかをチェックボックス（ReadOnly風）で表示
        // BeginDisabled/EndDisabled で囲むとエディタから誤操作できなくなります
        ImGui::BeginDisabled();
        ImGui::Checkbox("Is Occupied", &comp.isOccupied);
        ImGui::EndDisabled();

        // デバッグ用：強制解放ボタン（開発中にプレイヤーが詰まった時用）
        if (comp.isOccupied) {
            if (ImGui::Button("Force Release Spot")) {
                comp.isOccupied = false;
                changed = true;
            }
        }

        return changed;
    }
};

// コンポーネントの登録
REGISTER_COMPONENT(HidingSpotComponent, "HidingSpotComponent")

//============================================================================
//Hiding Spot Tags Meta
//============================================================================
// --- Under Tag ---
// --- Under Tag ---
template <> struct ComponentMeta<HidingUnder>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Hide Tag: Under";
    static constexpr bool        hasCustomGui = false;

    static const std::vector<FieldDescriptor>& Fields()
    {
        // 引数: Class, Member, JsonName, DisplayName, Speed, Min, Max, Category
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(HidingUnder, sinkDepth, "sinkDepth", "Sink Depth", 0.01f, 0.0f, 2.0f, "Movement"),
            META_FIELD_FLOAT(HidingUnder, lookDownAngle, "lookDownAngle", "Look Down Angle", 0.1f, -90.0f, 90.0f, "Camera")
        };
        return fields;
    }
};

// --- Open Tag ---
template <> struct ComponentMeta<HidingOpen>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Hide Tag: Open";
    static constexpr bool        hasCustomGui = false;

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(HidingOpen, waitTime, "waitTime", "Wait Time (sec)", 0.05f, 0.0f, 10.0f, "Sequence")
        };
        return fields;
    }
};

// --- TopIn Tag ---
template <> struct ComponentMeta<HidingTopIn>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Hide Tag: TopIn";
    static constexpr bool        hasCustomGui = false;

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(HidingTopIn, riseHeight, "riseHeight", "Rise Height", 0.01f, 0.0f, 2.0f, "Movement"),
            META_FIELD_FLOAT(HidingTopIn, lookDownAngle, "lookDownAngle", "Look Down Angle", 0.1f, -90.0f, 90.0f, "Camera"),
            META_FIELD_FLOAT(HidingTopIn, sinkDepth, "sinkDepth", "Sink Depth", 0.01f, 0.0f, 2.0f, "Movement")
        };
        return fields;
    }
};

//Tag
REGISTER_COMPONENT(HidingUnder, "HidingSpotTag_Under")
REGISTER_COMPONENT(HidingOpen, "HidingSpotTag_Open")
REGISTER_COMPONENT(HidingTopIn, "HidingSpotTag_TopIn")