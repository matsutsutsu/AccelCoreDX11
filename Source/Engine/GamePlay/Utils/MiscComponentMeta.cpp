#include "Engine/GamePlay/Graphics/Core/DissolveComponent.h"
#include "Engine/GamePlay/Graphics/Core/PrimitiveComponent.h"
#include "Engine/GamePlay/Utils/TimerComponent.h"

#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"


// ============================================================================
// Primitive Component
// ============================================================================
template <> struct ComponentMeta<PrimitiveComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Primitive Shape";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(PrimitiveComponent& p, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;

        const char* typeNames[] = { "Box", "Sphere", "Cylinder", "Capsule", "Cone" };
        int         currentType = static_cast<int>(p.type);

        if (ImGui::Combo("Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
            p.type = static_cast<PrimitiveType>(currentType);
            changed = true;
        }

        changed |= ImGui::ColorEdit4("Color", &p.color.x);
        changed |= ImGui::Checkbox("Wireframe", &p.isWireframe);

        ImGui::Separator();

        switch (p.type) {
        case PrimitiveType::Box:
            changed |= ImGui::DragFloat3("Size", &p.size.x, 0.1f, 0.0f, 100.0f);
            break;
        case PrimitiveType::Sphere:
            changed |= ImGui::DragFloat("Radius", &p.radius, 0.1f, 0.0f, 100.0f);
            break;
        case PrimitiveType::Cylinder:
        case PrimitiveType::Capsule:
        case PrimitiveType::Cone:
            changed |= ImGui::DragFloat("Radius", &p.radius, 0.1f, 0.0f, 100.0f);
            changed |= ImGui::DragFloat("Height", &p.height, 0.1f, 0.0f, 100.0f);
            break;
        }
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_ENUM_INT(PrimitiveComponent, type, "type", "Type", nullptr, 5, "Settings"),
            META_FIELD_FLOAT4(PrimitiveComponent, color, "color", "Color", 0.01f, "Settings"),
            META_FIELD_FLOAT3(PrimitiveComponent, size, "size", "Size", 0.1f, "Settings"),
            META_FIELD_FLOAT(PrimitiveComponent, radius, "radius", "Radius", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(PrimitiveComponent, height, "height", "Height", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_BOOL(PrimitiveComponent, isWireframe, "isWireframe", "Wireframe", "Settings") };
        return fields;
    }
};

// ============================================================================
// Dissolve Component
// ============================================================================
template <> struct ComponentMeta<DissolveComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Dissolve Effect";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(DissolveComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        ImGui::SeparatorText("Dissolve Settings");
        changed |= ImGui::SliderFloat("Current Threshold", &comp.currentThreshold, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Dissolve Speed", &comp.dissolveSpeed, 0.01f, 0.0f, 10.0f);
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(
                DissolveComponent, currentThreshold, "currentThreshold", "Threshold", 0.01f, 0.0f, 1.0f, "Settings"),
            META_FIELD_FLOAT(
                DissolveComponent, dissolveSpeed, "dissolveSpeed", "Speed", 0.01f, 0.0f, 10.0f, "Settings") };
        return fields;
    }
};

// ============================================================================
// Timer Component
// ============================================================================
template <> struct ComponentMeta<TimerComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Timer Component";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(TimerComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        ImGui::SeparatorText("Timer State");

        changed |= ImGui::DragFloat("Current Time (s)", &comp.currentTime, 0.01f, 0.0f, 10000.0f);
        changed |= ImGui::DragFloat("Life Time (s)", &comp.lifeTime, 0.1f, 0.0f, 10000.0f);

        ImGui::BeginDisabled();
        ImGui::Checkbox("Is Expired", &comp.isExpired);
        ImGui::EndDisabled();

        if (comp.isExpired && ImGui::Button("Reset Timer")) {
            comp.currentTime = 0.0f;
            comp.isExpired = false;
            changed = true;
        }

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(
                TimerComponent, currentTime, "currentTime", "Current Time", 0.01f, 0.0f, 0.0f, "Settings"),
            META_FIELD_FLOAT(TimerComponent, lifeTime, "lifeTime", "Life Time", 0.1f, 0.0f, 0.0f, "Settings"),
            META_FIELD_BOOL(TimerComponent, isExpired, "isExpired", "Is Expired", "Settings") };
        return fields;
    }
};

// ============================================================================
// 究極の自動化
// ============================================================================
REGISTER_COMPONENT(PrimitiveComponent, "Primitive")
REGISTER_COMPONENT(DissolveComponent, "Dissolve")
REGISTER_COMPONENT(TimerComponent, "Timer")