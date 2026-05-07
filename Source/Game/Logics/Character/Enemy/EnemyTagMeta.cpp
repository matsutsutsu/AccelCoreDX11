#include "EnemyTag.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"


// ============================================================================
// FPS Player Component Meta
// ============================================================================
template <> struct ComponentMeta<EnemyTag>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "EnemyTag";
    static constexpr bool        hasCustomGui = true;

    // JSONシリアライズ対象のフィールド定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // 入力設定（名前を保存）
            META_FIELD_BOOL(EnemyTag, CanLockON, "CanLockON", "CanLockON", "Config"),
        };
        return fields;
    }

    // インスペクター上でのカスタム描画（ドロップダウン選択）
    static bool CustomGui(EnemyTag& comp, unsigned long long/* entityID*/ = 0, void* /*world*/ = nullptr)
    {
        bool changed = false;

        ImGui::Checkbox("CanLockOnEnemy", &comp.CanLockON);

        return changed;
    }
};

// コンポーネントの自動登録
REGISTER_COMPONENT(EnemyTag, "EnemyTag")