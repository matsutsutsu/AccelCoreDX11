#include "Engine/GamePlay/Graphics/Lighting/FogComponent.h"
#include "Engine/GamePlay/Graphics/Lighting/ShadowMapConfigComponent.h"
#include "Engine/GamePlay/Graphics/PostProcess/BloomConfigComponent.h"
#include "Engine/GamePlay/Graphics/PostProcess/ToneMapConfigComponent.h"

#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Dialog.h"


// ======================================================
// ★診断コード: 問題解決後に削除する
// ======================================================
#include <Windows.h>
namespace {
    struct _DiagEnvMeta {
        _DiagEnvMeta() {
            OutputDebugStringA("[CCL_DIAG] EnvironmentComponentMeta.cpp の初期化が走った！\n");
        }
    } _diag_env_meta_instance;
}


// ============================================================================
// ToneMap Config
// ============================================================================
template <> struct ComponentMeta<ToneMapConfigComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Tone Mapping";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(ToneMapConfigComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        ImGui::SeparatorText("Exposure Settings");
        changed |= ImGui::DragFloat(u8"Exposure (露出)", &comp.exposure, 0.05f, 0.0f, 20.0f);
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(ToneMapConfigComponent, exposure, "exposure", "Exposure", 0.05f, 0.0f, 20.0f, "Settings") };
        return fields;
    }
};

// ============================================================================
// Bloom Config
// ============================================================================
template <> struct ComponentMeta<BloomConfigComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Bloom Effect";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(BloomConfigComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        ImGui::SeparatorText("Toggle");
        changed |= ImGui::Checkbox("Enable Bloom", &comp.enable);

        if (comp.enable) {
            ImGui::SeparatorText("Parameters");
            ImGui::Indent();
            changed |= ImGui::DragFloat(u8"Threshold (閾値)", &comp.threshold, 0.01f, 0.0f, 10.0f);
            changed |= ImGui::DragFloat(u8"Intensity (強度)", &comp.intensity, 0.1f, 0.0f, 40.0f);
            changed |= ImGui::DragFloat(u8"Soft Knee (滑らかさ)", &comp.softKnee, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat(u8"Radius (広がり)", &comp.radius, 0.01f, 0.0f, 5.0f);
            ImGui::Unindent();
        }
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_BOOL(BloomConfigComponent, enable, "enable", "Enable", "Toggle"),
            META_FIELD_FLOAT(BloomConfigComponent, threshold, "threshold", "Threshold", 0.01f, 0.0f, 10.0f, "Params"),
            META_FIELD_FLOAT(BloomConfigComponent, intensity, "intensity", "Intensity", 0.1f, 0.0f, 40.0f, "Params"),
            META_FIELD_FLOAT(BloomConfigComponent, softKnee, "softKnee", "Soft Knee", 0.01f, 0.0f, 1.0f, "Params"),
            META_FIELD_FLOAT(BloomConfigComponent, radius, "radius", "Radius", 0.01f, 0.0f, 5.0f, "Params") };
        return fields;
    }
};

// ============================================================================
// Fog Component
// ============================================================================
template <> struct ComponentMeta<FogComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Fog Settings";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(FogComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;

        changed |= ImGui::Checkbox(u8"Enable Fog (有効)", &comp.enabled);
        changed |= ImGui::ColorEdit4(u8"Base Color", &comp.color.x);

        if (ImGui::CollapsingHeader(u8"Distance & Height (距離・高さ)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat(u8"Start Dist", &comp.start, 0.5f);
            changed |= ImGui::DragFloat(u8"End Dist", &comp.end, 0.5f);
            changed |= ImGui::DragFloat(u8"Height Level (地面高さ)", &comp.heightStart, 0.1f);
            changed |= ImGui::DragFloat(u8"Height Density (濃さ)", &comp.heightDensity, 0.01f);
        }

        if (ImGui::CollapsingHeader(u8"Noise & Animation (揺らぎ)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text(u8"Noise Texture:");
            ImGui::SameLine();
            if (ImGui::Button("Load...##FogNoise")) {
                char        filename[256] = {};
                DialogConfig cfg;
                cfg.title = "Select Environment Map";
                cfg.filter = "HDR/DDS Files\0*.hdr;*.dds\0All Files\0*.*\0";
                cfg.defaultDir = "Assets/Environment";
                cfg.historyKey = "EnvironmentMap";

                if (Dialog::OpenFileName(filename, 256, cfg, Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                    strcpy_s(comp.noiseTexturePath, filename);
                    GpuResourceUtils::LoadTexture(Graphics::Instance().GetDevice(),
                        filename,
                        comp.noiseTextureSRV.ReleaseAndGetAddressOf());
                    changed = true;
                }
            }

            if (comp.noiseTextureSRV) {
                ImGui::Image((void*)comp.noiseTextureSRV.Get(), ImVec2(64, 64));
                ImGui::SameLine();
                if (ImGui::Button("Clear##FogNoise")) {
                    comp.noiseTextureSRV.Reset();
                    comp.noiseTexturePath[0] = '\0';
                    changed = true;
                }
            }

            changed |= ImGui::DragFloat(u8"Strength (強度)", &comp.noiseStrength, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat(u8"Scale (サイズ)", &comp.noiseScale, 0.001f);
            changed |= ImGui::DragFloat2(u8"Scroll Speed (流速)", &comp.noiseSpeed.x, 0.01f);
        }

        if (ImGui::CollapsingHeader(u8"Rim Light (Atmosphere)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::ColorEdit3(u8"Rim Color", &comp.rimColor.x);
            changed |= ImGui::DragFloat(u8"Strength", &comp.rimStrength, 0.1f, 0.0f, 10.0f);
            changed |= ImGui::DragFloat(u8"Power (鋭さ)", &comp.rimPower, 0.1f, 0.1f, 20.0f);
        }

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_BOOL(FogComponent, enabled, "enabled", "Enabled", "Base"),
            META_FIELD_FLOAT4(FogComponent, color, "color", "Color", 0.01f, "Base"),
            META_FIELD_FLOAT(FogComponent, start, "start", "Start Dist", 0.5f, 0.0f, 0.0f, "Dist"),
            META_FIELD_FLOAT(FogComponent, end, "end", "End Dist", 0.5f, 0.0f, 0.0f, "Dist"),
            META_FIELD_FLOAT(FogComponent, heightStart, "heightStart", "Height Start", 0.1f, 0.0f, 0.0f, "Height"),
            META_FIELD_FLOAT(
                FogComponent, heightDensity, "heightDensity", "Height Density", 0.01f, 0.0f, 1.0f, "Height"),
            META_FIELD_FLOAT(
                FogComponent, noiseStrength, "noiseStrength", "Noise Strength", 0.01f, 0.0f, 1.0f, "Noise"),
            META_FIELD_FLOAT(FogComponent, noiseScale, "noiseScale", "Noise Scale", 0.001f, 0.0f, 0.0f, "Noise"),
            META_FIELD_FLOAT2(FogComponent, noiseSpeed, "noiseSpeed", "Noise Speed", 0.01f, "Noise"),
            META_FIELD_FLOAT3(FogComponent, rimColor, "rimColor", "Rim Color", 0.01f, "Rim"),
            META_FIELD_FLOAT(FogComponent, rimStrength, "rimStrength", "Rim Strength", 0.1f, 0.0f, 10.0f, "Rim"),
            META_FIELD_FLOAT(FogComponent, rimPower, "rimPower", "Rim Power", 0.1f, 0.0f, 20.0f, "Rim")
        };
        return fields;
    }
};

// ============================================================================
// ShadowMap Config
// ============================================================================
template <> struct ComponentMeta<ShadowMapConfigComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "ShadowMapConfigComponent";

    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(ShadowMapConfigComponent& c, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        ImGui::SeparatorText("Shadow Settings");

        // &c.cascadeSplits.x を渡すことで、XMFLOAT3 を一気に操作できる
        changed |= ImGui::DragFloat3(u8"Cascade Splits (Near/Mid/Far)", &c.cascadeSplits.x, 1.0f, 1.0f, 1000.0f);

        changed |= ImGui::DragFloat(u8"Bias (縞模様対策)", &c.shadowBias, 0.0001f, 0.0f, 0.01f, "%.5f");
        changed |= ImGui::ColorEdit4(u8"Shadow Color", &c.shadowColor.x);
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
           META_FIELD_FLOAT(ShadowMapConfigComponent, shadowBias, "shadowBias", "Shadow Bias", 0.0001f, 0.0f, 0.01f, "Settings"),
            META_FIELD_FLOAT4(ShadowMapConfigComponent, shadowColor, "shadowColor", "Shadow Color", 0.01f, "Settings"),

            // さきほど定義したマクロを使って JSON の保存対象に組み込む
            META_FIELD_FLOAT3(ShadowMapConfigComponent, cascadeSplits, "cascadeSplits", "Cascade Splits", 1.0f, "Settings")
        };
        return fields;
    }
};

// ============================================================================
// 究極の自動化：これだけでJSONとImGuiの両方に登録される
// ============================================================================
REGISTER_COMPONENT(ToneMapConfigComponent, "ToneMapConfig")
REGISTER_COMPONENT(BloomConfigComponent, "BloomConfig")
REGISTER_COMPONENT(FogComponent, "Fog")
REGISTER_COMPONENT(ShadowMapConfigComponent, "ShadowMapConfig")