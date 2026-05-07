#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "MotionComponent.h" // パスは環境に合わせて調整してください
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

template <> struct ComponentMeta<MotionComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Motion (Dynamics)";

    static constexpr bool hasCustomGui = true;

    static bool CustomGui(MotionComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        using namespace DirectX;

        // --- 1. 基本データの表示・編集 ---
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Dynamics State");
        changed |= ImGui::DragFloat3("Velocity (m/s)", &comp.velocity.x, 0.1f);
        changed |= ImGui::DragFloat3("Acceleration", &comp.acceleration.x, 0.1f);
        changed |= ImGui::DragFloat3("Angular Velocity", &comp.angularVelocity.x, 0.1f);

        ImGui::Separator();

        // --- 2. パラメータ設定 ---
        ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "Parameters");
        changed |= ImGui::SliderFloat("Friction", &comp.friction, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Max Speed", &comp.maxSpeed, 0.5f, 0.0f, 1000.0f);

        ImGui::Separator();

        // --- 3. デバッグ用：リアルタイム加算ツール ---
        // エディタ実行中に「ちょっと飛ばしてみる」ための機能
        static XMFLOAT3 debugImpulse = { 0, 0, 0 };
        ImGui::Text("Test Impulse");
        ImGui::DragFloat3("##ImpulseVector", &debugImpulse.x, 0.1f);
        ImGui::SameLine();
        if (ImGui::Button("Add Impulse")) {
            comp.AddImpulse(debugImpulse);
            changed = true;
        }

        // 現在溜まっている移動予約値（読み取り専用として表示）
        ImGui::BeginDisabled();
        ImGui::LabelText("Pending Movement", "%.2f, %.2f, %.2f",
            comp.pendingMovement.x, comp.pendingMovement.y, comp.pendingMovement.z);
        ImGui::EndDisabled();

        return changed;
    }

    // JSONシリアライズ定義（pendingMovementなどの実行時キャッシュは保存しない）
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT3(MotionComponent, velocity, "velocity", "Velocity", 0.1f, "Motion"),
            META_FIELD_FLOAT3(MotionComponent, acceleration, "acceleration", "Acceleration", 0.1f, "Motion"),
            META_FIELD_FLOAT3(MotionComponent, angularVelocity, "angularVelocity", "Angular Velocity", 0.1f, "Motion"),
            META_FIELD_FLOAT(MotionComponent, friction, "friction", "Friction",0.01f,0,1.0f, "Motion"),
            META_FIELD_FLOAT(MotionComponent, maxSpeed, "maxSpeed", "Max Speed", 0.01f,0.0f,500.0f, "Motion")
        };
        return fields;
    }
};

// コンポーネント登録
REGISTER_COMPONENT(MotionComponent, "Motion")