#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

namespace {
    //  クォータニオン -> オイラー角 (Degree) への変換
    DirectX::XMFLOAT3 QuatToEulerDegrees(const DirectX::XMFLOAT4& q) {
        float pitch =
            std::asin(std::clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f));
        float yaw = std::atan2(2.0f * (q.w * q.y + q.z * q.x),
            1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        float roll = std::atan2(2.0f * (q.w * q.z + q.x * q.y),
            1.0f - 2.0f * (q.x * q.x + q.z * q.z));

        return { DirectX::XMConvertToDegrees(pitch), DirectX::XMConvertToDegrees(yaw),
                DirectX::XMConvertToDegrees(roll) };
    }

    //  オイラー角 (Degree) -> クォータニオンへの変換
    DirectX::XMFLOAT4 EulerDegreesToQuat(const DirectX::XMFLOAT3& euler) {
        DirectX::XMVECTOR qVec = DirectX::XMQuaternionRotationRollPitchYaw(
            DirectX::XMConvertToRadians(euler.x),
            DirectX::XMConvertToRadians(euler.y),
            DirectX::XMConvertToRadians(euler.z));
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, qVec);
        return result;
    }
} // namespace

template <> struct ComponentMeta<TransformComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Transform";

    //  直感的な編集のためカスタムGUIを有効化
    static constexpr bool hasCustomGui = true;

    static bool CustomGui(TransformComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;

        // 1. 位置
        if (ImGui::DragFloat3("Position", &comp.position.x, 0.1f)) {
            comp.isTeleported = true; // ★ImGuiで動かしたら物理エンジンにもワープを指示
            comp.isStatic = false;
			comp.isDirty = true; // ★行列の再計算も要求
            changed = true;
        }

        // 2. Rotation (回転をオイラー角で表示・編集)
        DirectX::XMFLOAT3 euler = QuatToEulerDegrees(comp.rotation);
        if (ImGui::DragFloat3("Rotation", &euler.x, 1.0f)) {
            comp.rotation = EulerDegreesToQuat(euler);
            comp.isTeleported = true; // ★回転も同様
            comp.isStatic = false;
            comp.isDirty = true; // ★行列の再計算も要求
            changed = true;
        }

        // 3. スケール
        if (ImGui::DragFloat3("Scale", &comp.scale.x, 0.1f)) {
            comp.isStatic = false;
            comp.isDirty = true; // ★行列の再計算も要求
            changed = true;
        }

        ImGui::Spacing();
        changed |= ImGui::Checkbox("Is Static (静的オブジェクト)", &comp.isStatic);

        return changed;
    }

    //  JSON自動シリアライズ用の定義（親兄弟のIDなどは保存しない）
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT4(TransformComponent, rotation, "rotation", "Rotation", 0.1f, "Transform"),
            META_FIELD_FLOAT3(TransformComponent, position, "position", "Position", 0.1f, "Transform"),
            META_FIELD_FLOAT3(TransformComponent, scale, "scale", "Scale", 0.1f, "Transform"),
            META_FIELD_BOOL(TransformComponent, isStatic, "isStatic", "Is Static", "Transform"),
            META_FIELD_ENTITY_ID(TransformComponent, parentID, "parentID", "Parent Entity ID", "Transform") };
        return fields;
    }
};

// ============================================================================
// 究極の自動化
// ============================================================================
REGISTER_COMPONENT(TransformComponent, "Transform")