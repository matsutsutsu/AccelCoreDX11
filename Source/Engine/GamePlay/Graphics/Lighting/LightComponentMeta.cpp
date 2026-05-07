#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/GamePlay/Graphics/Lighting/LightComponent.h"

// ============================================================================
// Directional Light
// ============================================================================
template <> struct ComponentMeta<DirectionalLightComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Directional Light";

    static constexpr bool hasCustomGui = true;

    static bool CustomGui(DirectionalLightComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        changed |= ImGui::ColorEdit3("Light Color", &comp.color.x);
        changed |= ImGui::DragFloat("Intensity", &comp.intensity, 0.05f, 0.0f, 100.0f);
        ImGui::Separator();
        changed |= ImGui::ColorEdit4("Sky Color", &comp.skyColor.x);
        changed |= ImGui::ColorEdit4("Ground Color", &comp.groundColor.x);
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT3(DirectionalLightComponent, color, "color", "Color", 0.01f, "Lighting"),
            META_FIELD_FLOAT(DirectionalLightComponent, intensity, "intensity", "Intensity", 0.05f, 0.0f, 100.0f, "Lighting"),
            META_FIELD_FLOAT4(DirectionalLightComponent, skyColor, "skyColor", "Sky Color", 0.01f, "Hemisphere"),
            META_FIELD_FLOAT4(DirectionalLightComponent, groundColor, "groundColor", "Ground Color", 0.01f, "Hemisphere") };
        return fields;
    }
};

// ============================================================================
// Point Light
// ============================================================================
template <> struct ComponentMeta<PointLightComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Point Light";

    static constexpr bool hasCustomGui = true;

    static bool CustomGui(PointLightComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        changed |= ImGui::ColorEdit3("Light Color", &comp.color.x);
        changed |= ImGui::DragFloat("Intensity", &comp.intensity, 0.05f, 0.0f, 100.0f);
        changed |= ImGui::DragFloat("Range", &comp.range, 0.5f, 0.0f, 1000.0f);
        ImGui::Separator();
        ImGui::Text("Attenuation (減衰パラメータ)");
        changed |= ImGui::DragFloat("Constant", &comp.constant, 0.01f, 0.0f, 10.0f);
        changed |= ImGui::DragFloat("Linear", &comp.linear, 0.01f, 0.0f, 10.0f);
        changed |= ImGui::DragFloat("Quadratic", &comp.quadratic, 0.001f, 0.0f, 1.0f);
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT3(PointLightComponent, color, "color", "Color", 0.01f, "Lighting"),
            META_FIELD_FLOAT(PointLightComponent, intensity, "intensity", "Intensity", 0.05f, 0.0f, 100.0f, "Lighting"),
            META_FIELD_FLOAT(PointLightComponent, range, "range", "Range", 0.5f, 0.0f, 1000.0f, "Lighting"),
            META_FIELD_FLOAT(PointLightComponent, constant, "constant", "Constant", 0.01f, 0.0f, 10.0f, "Attenuation"),
            META_FIELD_FLOAT(PointLightComponent, linear, "linear", "Linear", 0.01f, 0.0f, 10.0f, "Attenuation"),
            META_FIELD_FLOAT(PointLightComponent, quadratic, "quadratic", "Quadratic", 0.001f, 0.0f, 1.0f, "Attenuation") };
        return fields;
    }
};

// ============================================================================
// Spot Light
// ============================================================================
template <> struct ComponentMeta<SpotLightComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Spot Light";

    static constexpr bool hasCustomGui = true;

    static bool CustomGui(SpotLightComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        changed |= ImGui::ColorEdit3("Light Color", &comp.color.x);
        changed |= ImGui::DragFloat("Intensity", &comp.intensity, 0.05f, 0.0f, 100.0f);
        changed |= ImGui::DragFloat("Range", &comp.range, 0.5f, 0.0f, 1000.0f);

        ImGui::Separator();
        ImGui::Text("Cone Angles (Cos Values)");

        // SpotLight特有の角度調整 (0.0が直角方向、1.0が真正面)
        changed |= ImGui::SliderFloat("Inner Cone Cos", &comp.innerCos, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Outer Cone Cos", &comp.outerCos, 0.0f, 1.0f);

        // 【フェイルセーフ】Inner(明るい部分)がOuter(減衰の境界)より小さくならないように強制補正
        if (comp.innerCos < comp.outerCos) {
            comp.innerCos = comp.outerCos;
            changed = true;
        }

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT3(SpotLightComponent, color, "color", "Color", 0.01f, "Lighting"),
            META_FIELD_FLOAT(SpotLightComponent, intensity, "intensity", "Intensity", 0.05f, 0.0f, 100.0f, "Lighting"),
            META_FIELD_FLOAT(SpotLightComponent, range, "range", "Range", 0.5f, 0.0f, 1000.0f, "Lighting"),
            META_FIELD_FLOAT(SpotLightComponent, innerCos, "innerCos", "Inner Cone (Cos)", 0.01f, 0.0f, 1.0f, "Cone"),
            META_FIELD_FLOAT(SpotLightComponent, outerCos, "outerCos", "Outer Cone (Cos)", 0.01f, 0.0f, 1.0f, "Cone") };
        return fields;
    }
};

// ============================================================================
// 究極の自動化：これだけでJSONとImGuiの両方に登録される
// ============================================================================
REGISTER_COMPONENT(DirectionalLightComponent, "DirectionalLight")
REGISTER_COMPONENT(PointLightComponent, "PointLight")
REGISTER_COMPONENT(SpotLightComponent, "SpotLight")