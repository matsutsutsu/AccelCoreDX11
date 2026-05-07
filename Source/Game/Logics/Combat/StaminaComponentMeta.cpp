//4/16桃田作成

#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// プレイヤーコンポーネントと入力API
#include "StaminaComponent.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "Engine/Platform/Input/InputFacade.h" // GetAxisNames等を使用する場合
#include "ECS/Core/CCL_World.h"

#include "Game/Logic/System/Modifier/ModifierComponent.h"

// ============================================================================
// Stamina Component Meta
// ============================================================================
template <> struct ComponentMeta<StaminaComponent>
{
    static constexpr bool    registered = true;
    static constexpr const char* displayName = "Stamina System";
    static constexpr bool    hasCustomGui = true;

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // 基本設定
            META_FIELD_FLOAT(StaminaComponent, maxStamina, "maxStamina", "Max Stamina", 1.0f, 0.0f, 1000.0f, "Settings"),
            META_FIELD_FLOAT(StaminaComponent, recoveryRateBase, "recoveryRate", "Recovery Rate", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(StaminaComponent, consumeRate, "consumeRate", "Consume Rate", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(StaminaComponent, recoveryDelayTime, "recDelayTime", "Recovery Delay Time", 0.1f, 0.0f, 5.0f, "Settings"),
            META_FIELD_FLOAT(StaminaComponent, fatigueRecoveryThreshold, "fatigueThreshold", "Fatigue Threshold", 0.01f, 0.0f, 1.0f, "Settings"),

            // 補正倍率
            META_FIELD_FLOAT(StaminaComponent, moveRecoveryMultiplier, "moveRecMult", "Moving Recovery Mult", 0.01f, 0.0f, 1.0f, "Multipliers"),

            // ★ 疲労デバフ設定 (Fatigue Modifiers)
            // --- ★ 疲労デバフ設定: 基本 (Fatigue Settings) ---
            META_FIELD_FLOAT(StaminaComponent, fatigueMoveSpeedMult, "fMoveSpeedMult", "Fatigue: Move Speed Mult", 0.01f, 0.0f, 1.0f, "Fatigue Settings"),
            META_FIELD_FLOAT(StaminaComponent, fatigueRecoveryMultiplier, "fRecMult", "Fatigue: Recovery Mult", 0.01f, 0.0f, 1.0f, "Fatigue Settings"),
            META_FIELD_FLOAT(StaminaComponent, fatigueFOVMult, "fFOVMult", "Fatigue: FOV Mult", 0.01f, 0.0f, 1.5f, "Fatigue Settings"),
            
            // --- ★ 疲労デバフ設定: 停止時演出 (Fatigue Idle) ---
            META_FIELD_FLOAT(StaminaComponent, fatigueIdleBobAmountMult, "fIdleBobAmt", "Fatigue: Idle Bob Amount", 0.1f, 0.0f, 5.0f, "Fatigue Idle"),
            META_FIELD_FLOAT(StaminaComponent, fatigueIdleBobSpeedMult, "fIdleBobSpd", "Fatigue: Idle Bob Speed", 0.1f, 0.0f, 5.0f, "Fatigue Idle"),
            META_FIELD_FLOAT(StaminaComponent, fatigueIdleTiltMult, "fIdleTilt", "Fatigue: Idle Tilt Mult", 0.1f, 0.0f, 5.0f, "Fatigue Idle"),
            
            // --- ★ 疲労デバフ設定: 歩行時演出 (Fatigue Walk) ---
            META_FIELD_FLOAT(StaminaComponent, fatigueWalkBobAmountMult, "fWalkBobAmt", "Fatigue: Walk Bob Amount", 0.1f, 0.0f, 5.0f, "Fatigue Walk"),
            META_FIELD_FLOAT(StaminaComponent, fatigueWalkBobSpeedMult, "fWalkBobSpd", "Fatigue: Walk Bob Speed", 0.1f, 0.0f, 5.0f, "Fatigue Walk"),
            META_FIELD_FLOAT(StaminaComponent, fatigueWalkTiltMult, "fWalkTilt", "Fatigue: Walk Tilt Mult", 0.1f, 0.0f, 5.0f, "Fatigue Walk"),
        };
        return fields;
    }

    static bool CustomGui(StaminaComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;

        auto worldPtr = static_cast<CCL::ECS::Core::World*>(world);

        // --- 1. Live Status (スタミナバー) ---
        ImGui::SeparatorText("Live Status");

        // --- 1. スタミナ計算 ---
        float baseMax = comp.maxStamina;
        float bonusMax = 0.0f;
        if (worldPtr) {
            if (auto Mod = worldPtr->GetComponent<ModifierStatusComponent>(entityID)) {
                bonusMax = Mod->staminaMaxAdd;
            }
        }

        float totalMax = baseMax + bonusMax;
        if (totalMax <= 0.0f) totalMax = 1.0f;

        // --- 2. 描画位置とサイズの取得 ---
        ImVec2 barSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 1.5f);
        ImVec2 p0 = ImGui::GetCursorScreenPos();          // バーの左上座標
        ImVec2 p1 = ImVec2(p0.x + barSize.x, p0.y + barSize.y); // バーの右下座標
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 各セグメントの「右端」の座標を計算
        float baseEdgeX = p0.x + (baseMax / totalMax) * barSize.x;
        float currentX = p0.x + (comp.current / totalMax) * barSize.x;

        // --- 3. 標準の ProgressBar で「背景」と「ベース枠」を確保 ---
        // 中身は描かず、枠と背景だけ表示させるために 0.0f を渡す
        ImGui::ProgressBar(0.0f, barSize, "");

        // --- 4. カスタム描画 (DrawListを使用) ---
        // A. 黄色いバー (0 ～ baseMax の範囲)
        float yellowRight = (std::min)(currentX, baseEdgeX);
        if (yellowRight > p0.x) {
            ImU32 yellowCol = comp.isFatigued ? IM_COL32(255, 50, 50, 255) : IM_COL32(255, 200, 0, 255);
            drawList->AddRectFilled(p0, ImVec2(yellowRight, p1.y), yellowCol, ImGui::GetStyle().FrameRounding);
        }

        // B. 青色のバー (baseMax ～ totalMax の範囲)
        if (comp.current > baseMax) {
            float blueLeft = baseEdgeX;
            float blueRight = (std::min)(currentX, p1.x);

            // 追加分だけを青色で描画
            ImU32 blueCol = IM_COL32(50, 150, 255, 255);
            drawList->AddRectFilled(ImVec2(blueLeft, p0.y), ImVec2(blueRight, p1.y), blueCol, ImGui::GetStyle().FrameRounding);
        }

        // --- 5. テキストを一番上に描画 ---
        char buf[128];
        sprintf_s(buf, "Stamina: %.1f / %.1f %s", comp.current, totalMax, comp.isFatigued ? "[FATIGUED]" : "");
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        drawList->AddText(ImVec2(p0.x + (barSize.x - textSize.x) * 0.5f, p0.y + (barSize.y - textSize.y) * 0.5f),
            IM_COL32(255, 255, 255, 255), buf);

        // 次の項目のためにカーソルを下に移動
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + barSize.y + ImGui::GetStyle().ItemSpacing.y));

        // --- 2. 回復ロックタイマー ---
        if (comp.recoveryDelayTimer > 0.001f)
        {
            float delayFraction = comp.recoveryDelayTimer / comp.recoveryDelayTime;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            char delayBuf[64];
            sprintf_s(delayBuf, "Recovery Lock: %.2fs", comp.recoveryDelayTimer);
            ImGui::ProgressBar(delayFraction, ImVec2(-1.0f, 12.0f), delayBuf);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Recovery Active");
        }

        // --- 3. 状態フラグ (Read Only) ---
        ImGui::BeginDisabled();
        ImGui::Columns(3, "state_cols", false);
        ImGui::Checkbox("Moving", &comp.isMoving); ImGui::NextColumn();
        ImGui::Checkbox("Consuming", &comp.isConsuming); ImGui::NextColumn();
        ImGui::Checkbox("Fatigued", &comp.isFatigued); ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::EndDisabled();

        // --- 4. パラメータ設定 (Reflection) ---
        ImGui::SeparatorText("Stamina Settings");

        // カテゴリごとに分けて表示すると見やすいですが、
        // 今回はシンプルに Fields を回します。
        for (const auto& fd : Fields()) {
            if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
        }

        return changed;
    }
};

// コンポーネントの自動登録
REGISTER_COMPONENT(StaminaComponent, "StaminaComponent")