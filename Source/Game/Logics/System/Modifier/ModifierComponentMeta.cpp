#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// プレイヤーコンポーネントと入力API
#include "ModifierComponent.h"

// ============================================================================
// Modifier Component Meta
// ============================================================================
template <> struct ComponentMeta<ModifierComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Modifier Manager";
    static constexpr bool        hasCustomGui = true;

    // シリアライズ対象：動的なバフは保存しないため空にする
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {};
        return fields;
    }

    // インスペクター描画
    static bool CustomGui(ModifierComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;

        // --- 1. 現在有効な Modifier の一覧表示 ---
        ImGui::SeparatorText("Active Modifiers");

        if (comp.activeMods.empty()) {
            ImGui::TextDisabled("No active modifiers.");
        }
        else {
            // カラムを6列に拡張 (Nameを追加)
            if (ImGui::BeginTable("ModTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Target");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Time");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)comp.activeMods.size(); ++i) {
                    auto& mod = comp.activeMods[i];
                    ImGui::TableNextRow();

                    // 名前
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(mod.name[0] != '\0' ? mod.name : "(No Name)");

                    // ターゲット
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(GetParamTypeName(mod.target));

                    // 値
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.2f", mod.value);

                    // 単位
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(mod.isPercent ? "Mult (*)" : "Flat (+)");

                    // 残り時間
                    ImGui::TableSetColumnIndex(4);
                    if (mod.IsPermanent()) {
                        ImGui::TextColored(ImVec4(1, 1, 0, 1), "PERM");
                    }
                    else {
                        ImGui::Text("%.1fs", mod.duration);
                    }

                    // 削除ボタン
                    ImGui::TableSetColumnIndex(5);
                    ImGui::PushID(i);
                    if (ImGui::Button("X")) {
                        comp.activeMods.erase(comp.activeMods.begin() + i);
                        i--;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        if (!comp.activeMods.empty()) {
            if (ImGui::Button("Remove All Modifiers")) {
                comp.activeMods.clear();
                changed = true;
            }
        }

        // --- 2. デバッグ用：Modifier の手動生成・追加 ---
        ImGui::Spacing();
        ImGui::SeparatorText("Debug: Add New Modifier");

        static char addName[32] = "Debug Buff";
        static int selectedTarget = 0;
        static float addValRaw = 20.0f;
        static float addDuration = 5.0f;
        static bool addIsPercent = true;
        static bool addIsPermanent = false;
        static int lastSelectedTarget = -1; // 変更検知用

        ImGui::InputText("Buff Name", addName, IM_ARRAYSIZE(addName));

        const char* targetNames[] = {
            "Move_WalkSpeed", "Move_RunSpeed", "Move_Accel", "Move_Overall",
            "Stam_Max", "Stam_Consume", "Stam_Recover", "Stam_Delay", "Stam_FatigueThreshold",
            "View_FOV",
            "View_IdleBobAmt", "View_IdleBobSpd",
            "View_WalkBobAmt", "View_WalkBobSpd",
            "View_RunBobAmt", "View_RunBobSpd",
            "View_RunTilt", "View_FatigueTiltMult"
        };

        // ターゲットが変更されたら、自動で加算・割合を切り替える
        if (ImGui::Combo("Target", &selectedTarget, targetNames, IM_ARRAYSIZE(targetNames))) {
            PlayerMod::ParamType type = static_cast<PlayerMod::ParamType>(selectedTarget);

            // Stamina(4~8番目) と FOV(9番目) の判定
            // (列挙型の定義順に依存しますが、このComboの並び順で判定します)
            bool isStamina = (selectedTarget >= 4 && selectedTarget <= 8);
            bool isFOV = (selectedTarget == 9);

            if (isStamina || isFOV) {
                addIsPercent = false; // 強制的に固定値(+)モードへ
                if (lastSelectedTarget != selectedTarget) addValRaw = 20.0f; // デフォルト値を加算用に
            }
            else {
                addIsPercent = true;  // それ以外は割合(%)モードへ
                if (lastSelectedTarget != selectedTarget) addValRaw = 20.0f; // 1.2倍相当
            }
            lastSelectedTarget = selectedTarget;
        }

        // --- 入力フィールドの表示 ---
        if (addIsPercent) {
            ImGui::DragFloat("補正量 %", &addValRaw, 1.0f, -100.0f, 1000.0f, "%.0f%%");
            ImGui::SameLine();
            ImGui::TextDisabled("(x%.2f)", 1.0f + (addValRaw / 100.0f));
        }
        else {
            ImGui::DragFloat("補正値 (固定)", &addValRaw, 0.1f, -500.0f, 500.0f, "%+.1f");
        }

        ImGui::BeginDisabled(addIsPermanent);
        ImGui::DragFloat("持続時間 (秒)", &addDuration, 0.1f, 0.1f, 3600.0f);
        ImGui::EndDisabled();

        // Stamina/FOV のときは Checkbox を無効化（固定値強制）
        bool forceFlat = (selectedTarget >= 4 && selectedTarget <= 9);
        ImGui::BeginDisabled(forceFlat);
        ImGui::Checkbox("割合(%)で適用", &addIsPercent);
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Checkbox("永続的に適用", &addIsPermanent)) {
            addDuration = addIsPermanent ? -1.0f : 5.0f;
        }

        if (ImGui::Button("Add to Entity")) {
            PlayerMod::ParamType type = static_cast<PlayerMod::ParamType>(selectedTarget);
            float duration = addIsPermanent ? -1.0f : addDuration;

            float finalValueToPass = addValRaw;
            if (addIsPercent) {
                // 割合モードなら内部倍率(1.2)に変換
                finalValueToPass = 1.0f + (addValRaw / 100.0f);
            }
            // 固定値モードなら入力値をそのまま(20.0)渡す

            PlayerMod::AddModifier(&comp, addName, type, 9999, finalValueToPass, duration, addIsPercent);
            changed = true;
        }
        return changed;
    }

private:
    static const char* GetParamTypeName(PlayerMod::ParamType type) {
        switch (type) {
        case PlayerMod::ParamType::Move_WalkSpeed:    return "WalkSpeed";
        case PlayerMod::ParamType::Move_RunSpeed:     return "RunSpeed";
        case PlayerMod::ParamType::Move_Acceleration: return "Accel";
        case PlayerMod::ParamType::Move_OverallSpeed: return "OVERALL";
        case PlayerMod::ParamType::View_IdleBobAmount: return "IdleBobAmt";
        case PlayerMod::ParamType::View_IdleBobSpeed:  return "IdleBobSpd";
        case PlayerMod::ParamType::View_WalkBobAmount: return "WalkBobAmt";
        case PlayerMod::ParamType::View_WalkBobSpeed:  return "WalkBobSpd";
        case PlayerMod::ParamType::View_RunBobAmount:  return "RunBobAmt";
        case PlayerMod::ParamType::View_RunBobSpeed:   return "RunBobSpd";
        case PlayerMod::ParamType::View_RunTiltAmount: return "RunTilt";
        case PlayerMod::ParamType::View_FatigueTiltMult: return "FatigueTilt";
        default: return "Other";
        }
    }
};

template <> struct ComponentMeta<ModifierStatusComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Modifier Status (Read Only)";
    static constexpr bool        hasCustomGui = true;

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {};
        return fields;
    }

    static bool CustomGui(ModifierStatusComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        // 基本的に閲覧専用なので changed は常に false32s
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 1.0f, 1.0f)); // デバッグ値っぽく少し青みがかった色に

        if (ImGui::TreeNodeEx("Movement Multipliers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("Walk Speed:  x%.2f", comp.moveWalkSpeedMult);
            ImGui::BulletText("Run Speed:   x%.2f", comp.moveRunSpeedMult);
            ImGui::BulletText("Acceleration: x%.2f", comp.moveAccelMult);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Stamina Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("Max Stamina:  +%.1f", comp.staminaMaxAdd);
            ImGui::BulletText("Consume Rate: x%.2f", comp.staminaConsumeMult);
            ImGui::BulletText("Recover Rate: x%.2f", comp.staminaRecoverMult);
            ImGui::BulletText("Recov Delay:  +%.2fs", comp.staminaRecoverDelayAdd);
            ImGui::BulletText("Fatigue Thresh: +%.2f", comp.staminaFatigueThresholdAdd);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("View Multipliers (Detailed)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("FOV Add: +%.1f", comp.viewBaseFOVAdd);

            if (ImGui::TreeNode("Bobbing (Idle/Walk/Run)")) {
                ImGui::BulletText("Idle: Amt(x%.2f) Spd(x%.2f)", comp.viewIdleBobAmountMult, comp.viewIdleBobSpeedMult);
                ImGui::BulletText("Walk: Amt(x%.2f) Spd(x%.2f)", comp.viewWalkBobAmountMult, comp.viewWalkBobSpeedMult);
                ImGui::BulletText("Run : Amt(x%.2f) Spd(x%.2f)", comp.viewRunBobAmountMult, comp.viewRunBobSpeedMult);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Tilt & Fatigue Effects")) {
                ImGui::BulletText("Run Tilt: x%.2f", comp.viewRunTiltAmountMult);
                ImGui::BulletText("Fatigue Tilt Add: +%.2f", comp.viewFatigueTiltMult);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleColor();

        return false;
    }
};

REGISTER_COMPONENT(ModifierComponent, "ModifierComponent")
REGISTER_COMPONENT(ModifierStatusComponent, "ModifierStatusComponent")
