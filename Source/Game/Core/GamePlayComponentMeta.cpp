//// (必要なヘッダーをインクルードしてください。Health, Player, Trigger, LuaScript 等)
//#include "Editor/Inspector/ComponentGuiRegistry.h"
//#include "Engine/Graphics/Core/Graphics.h"
//#include "Engine/Platform/Dialog.h"
//#include "Engine/Scripting/LuaScriptComponent.h"
//#include "Engine/Serialization/ComponentRegistry.h"
//#include "Engine/Serialization/Meta/ComponentMeta.h"
//#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
//#include "Engine/Serialization/Meta/ComponentMetaJson.h"
//#include "Game/Logic/Combat/HealthComponent.h"
//#include "Game/Logic/World/Triggers/TriggerComponent.h"
//#include <algorithm>
//
//// ============================================================================
//// Health Component
//// ============================================================================
//template <> struct ComponentMeta<HealthComponent> {
//    static constexpr bool        registered   = true;
//    static constexpr const char *displayName  = "Health & Damage";
//    static constexpr bool        hasCustomGui = true;
//
//    static bool CustomGui(HealthComponent &h)
//    {
//        bool changed = false;
//
//        ImGui::SeparatorText("Status");
//        float fraction = (h.maxHealth > 0.0f) ? (h.currentHealth / h.maxHealth) : 0.0f;
//        char  hpLabel[32];
//        sprintf_s(hpLabel, "%.0f / %.0f", h.currentHealth, h.maxHealth);
//
//        // HPバーの色変化
//        if (fraction > 0.6f)
//            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
//        else if (fraction > 0.3f)
//            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
//        else
//            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
//
//        ImGui::ProgressBar(fraction, ImVec2(-1, 0), hpLabel);
//        ImGui::PopStyleColor();
//
//        changed |= ImGui::DragFloat("Max HP", &h.maxHealth, 1.0f, 1.0f, 10000.0f);
//        changed |= ImGui::SliderFloat("Current HP", &h.currentHealth, 0.0f, h.maxHealth);
//
//        ImGui::SeparatorText("Invincibility");
//        if (h.invincibilityDuration > 0.0f) {
//            float invFraction =
//                std::clamp(h.invincibilityTimer / h.invincibilityDuration, 0.0f, 1.0f);
//            ImGui::ProgressBar(invFraction, ImVec2(-1, 0), "Invincible Timer");
//        }
//        changed |= ImGui::DragFloat("Duration (sec)", &h.invincibilityDuration, 0.1f, 0.0f, 10.0f);
//
//        // チーム設定
//        const char *teams[]     = {"Player", "Enemy", "Neutral"};
//        int         currentTeam = static_cast<int>(h.team);
//        if (ImGui::Combo("Team", &currentTeam, teams, IM_ARRAYSIZE(teams))) {
//            h.team  = static_cast<TeamID>(currentTeam);
//            changed = true;
//        }
//
//        return changed;
//    }
//
//    static const std::vector<FieldDescriptor> &Fields()
//    {
//        static const std::vector<FieldDescriptor> fields = {
//            META_FIELD_FLOAT(HealthComponent,
//                maxHealth,
//                "maxHealth",
//                "Max Health",
//                1.0f,
//                1.0f,
//                10000.0f,
//                "Status"),
//            META_FIELD_FLOAT(HealthComponent,
//                currentHealth,
//                "currentHealth",
//                "Current Health",
//                1.0f,
//                0.0f,
//                10000.0f,
//                "Status"),
//            META_FIELD_FLOAT(HealthComponent,
//                invincibilityDuration,
//                "invincibilityDuration",
//                "Invincible Duration",
//                0.1f,
//                0.0f,
//                10.0f,
//                "Status"),
//            META_FIELD_ENUM_INT(
//                HealthComponent, team, "team", "Team", nullptr, 3, "Status") // EnumNamesは適宜設定
//        };
//        return fields;
//    }
//};
//
//// ============================================================================
//// Trigger Component
//// ============================================================================
//template <> struct ComponentMeta<TriggerComponent> {
//    static constexpr bool        registered   = true;
//    static constexpr const char *displayName  = "Trigger Area";
//    static constexpr bool        hasCustomGui = true;
//
//    static bool CustomGui(TriggerComponent &t)
//    {
//        bool changed = false;
//        ImGui::SeparatorText("Trigger Settings");
//        changed |= ImGui::DragFloat(u8"Radius (半径)", &t.radius, 0.1f, 0.0f, 100.0f);
//        changed |= ImGui::Checkbox(u8"One Shot (一回のみ)", &t.isOneShot);
//
//        ImGui::SeparatorText("Runtime State");
//        ImGui::BeginDisabled();
//        ImGui::Checkbox(u8"Has Triggered", &t.hasTriggered);
//        ImGui::EndDisabled();
//
//        if (t.hasTriggered && ImGui::Button("Reset Trigger")) {
//            t.hasTriggered = false;
//            changed        = true;
//        }
//        return changed;
//    }
//
//    static const std::vector<FieldDescriptor> &Fields()
//    {
//        static const std::vector<FieldDescriptor> fields = {
//            META_FIELD_FLOAT(
//                TriggerComponent, radius, "radius", "Radius", 0.1f, 0.0f, 100.0f, "Area"),
//            META_FIELD_BOOL(TriggerComponent, isOneShot, "isOneShot", "One Shot", "Area")};
//        return fields;
//    }
//};
//
//// ============================================================================
//// Lua Script Component
//// ============================================================================
//template <> struct ComponentMeta<LuaScriptComponent> {
//    static constexpr bool        registered   = true;
//    static constexpr const char *displayName  = "Lua Script";
//    static constexpr bool        hasCustomGui = true;
//
//    static bool CustomGui(LuaScriptComponent &s)
//    {
//        bool changed = false;
//        ImGui::SeparatorText("Script File");
//
//        // パスを直接入力できるように改良
//        char buf[256];
//        strcpy_s(buf, s.scriptPath.c_str());
//        if (ImGui::InputText("Path", buf, sizeof(buf))) {
//            s.scriptPath = buf;
//            s.isLoaded   = false;
//            changed      = true;
//        }
//
//        if (ImGui::Button("Load...##Lua")) {
//            char filename[256] = {};
//            if (Dialog::OpenFileName(filename,
//                    256,
//                    "Lua\0*.lua\0All\0*.*\0",
//                    "Select Script",
//                    GetActiveWindow()) == DialogResult::OK) {
//                // 絶対パスから相対パスへの変換 (既存のロジックを踏襲)
//                namespace fs = std::filesystem;
//                std::error_code ec;
//                fs::path        relPath = fs::relative(filename, fs::current_path(), ec);
//                s.scriptPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;
//                s.isLoaded   = false;
//                changed      = true;
//            }
//        }
//        ImGui::SameLine();
//        if (ImGui::Button("Force Reload")) {
//            s.isLoaded = false;
//            changed    = true;
//        }
//
//        ImGui::TextColored(
//            s.isLoaded ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
//            s.isLoaded ? "Status: Loaded" : "Status: Waiting load...");
//        return changed;
//    }
//
//    // std::stringのシリアライズは既存の FD_String (または追加したマクロ) を使用
//    static const std::vector<FieldDescriptor> &Fields()
//    {
//        static const std::vector<FieldDescriptor> fields = {
//             META_FIELD_STRING(LuaScriptComponent, scriptPath, "scriptPath", "Script Path",
//             "File") //
//            // ※マクロがあれば使用
//        };
//        return fields;
//    }
//};
//
//void RegisterGamePlayMeta()
//{
//    auto &jsonReg = ComponentRegistry::Instance();
//    ComponentMetaJson::RegisterMetaT<HealthComponent>(jsonReg, "Health");
//    ComponentMetaJson::RegisterMetaT<TriggerComponent>(jsonReg, "Trigger");
//    ComponentMetaJson::RegisterMetaT<LuaScriptComponent>(jsonReg, "LuaScript");
//
//    auto &guiReg = ComponentGuiRegistry::Instance();
//    ComponentMetaImGui::RegisterGuiMeta<HealthComponent>(guiReg);
//    ComponentMetaImGui::RegisterGuiMeta<TriggerComponent>(guiReg);
//    ComponentMetaImGui::RegisterGuiMeta<LuaScriptComponent>(guiReg);
//}