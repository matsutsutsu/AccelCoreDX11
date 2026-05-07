#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// プレイヤーコンポーネントと入力API
#include "TPSPlayerComponent.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "Engine/Platform/Input/InputFacade.h" // GetAxisNames等を使用する場合
#include "ECS/Core/CCL_World.h"

#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Core/NameComponent.h" // NameComponentの定義ヘッダー

// ============================================================================
// FPS Player Component Meta
// ============================================================================
template <> struct ComponentMeta<TPSPlayerComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "TPS Player Controller";
    static constexpr bool        hasCustomGui = true;

  

    // JSONシリアライズ対象のフィールド定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_STRING(TPSPlayerComponent, config.axis.moveFB, "moveFB", "Move FB Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.axis.moveLR, "moveLR", "Move LR Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.sprint, "sprintKey", "sprint Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.dodge, "dodgeKey", "dodge Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.guard, "guardKey", "guard Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.jump, "jumpKey", "jump Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.Targeting, "TargetingKey", "Targeting Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.decide, "DecideKey", "Decide Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.lockOn, "LockOnKey", "LockOn Action", "Input Settings"),
            META_FIELD_STRING(TPSPlayerComponent, config.action.lockOnChenge, "LockOnChenge", "LockOnChenge Action", "Input Settings"),

            META_FIELD_FLOAT(TPSPlayerComponent, walkSpeed, "walkSpeed", "Walk Speed", 0.1f, 0.0f, 20.0f, "Movement"),
            META_FIELD_FLOAT(TPSPlayerComponent, runSpeed, "runSpeed", "Run Speed", 0.1f, 0.0f, 40.0f, "Movement"),
            META_FIELD_FLOAT(TPSPlayerComponent, rotationSpeed, "rotationSpeed", "RotationSpeed", 0.01f, 0.0f, 1.0f, "Movement"),
        };
        return fields;
    }

    // インスペクター上でのカスタム描画（ドロップダウン選択）
    static bool CustomGui(TPSPlayerComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;
        auto* worldPtr = static_cast<CCL::ECS::Core::World*>(world);

        ImGui::InputFloat("currentSpeed", &comp.currentSpeed);
        ImGui::InputFloat("moveForward", &comp.input.moveInput.y);
        ImGui::InputFloat("moveRight", &comp.input.moveInput.x);

        // Input API の取得（リソースから取得）
        std::shared_ptr<IInputAPI> inputAPI = nullptr;
        if (worldPtr && worldPtr->HasResource<std::shared_ptr<IInputAPI>>()) {
            inputAPI = worldPtr->GetResource<std::shared_ptr<IInputAPI>>();
        }



        // --- 1. 移動パラメータの描画 (標準機能を利用) ---
        ImGui::SeparatorText("Movement Stats");
        for (const auto& fd : Fields()) {
            // カテゴリが "Movement" のものだけ先に描画
            if (std::string(fd.category) == "Movement") {
                if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
            }
        }

        // --- 2. 入力設定の描画 (ドロップダウン化) ---
        ImGui::SeparatorText("Input Mapping");

        // InputFacadeが取得できている場合のみドロップダウンを表示
        auto* facade = dynamic_cast<InputFacade*>(inputAPI.get());

        if (!facade) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: Facade is Null!");
        }
        else {
            ImGui::Text("Axis Count: %d", (int)facade->GetAxisNames().size());
        }

        if (facade) {

            // Axis用のヘルパー描画
            auto DrawAxisCombo = [&](const char* label, std::string& target) {
                if (ImGui::BeginCombo(label, target.c_str())) {
                    for (const auto& name : facade->GetAxisNames()) {
                        if (ImGui::Selectable(name.c_str(), target == name)) {
                            target = name;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                };

            // Action用のヘルパー描画
            auto DrawActionCombo = [&](const char* label, std::string& target) {
                if (ImGui::BeginCombo(label, target.c_str())) {
                    for (const auto& name : facade->GetActionNames()) {
                        if (ImGui::Selectable(name.c_str(), target == name)) {
                            target = name;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                };

            DrawAxisCombo("Move Forward/Back", comp.config.axis.moveFB);
            DrawAxisCombo("Move Left/Right", comp.config.axis.moveLR);
            DrawActionCombo("Dash Action", comp.config.action.sprint);
            DrawActionCombo("Attack Action", comp.config.action.attack);
            DrawActionCombo("Dodge Action", comp.config.action.dodge);
            DrawActionCombo("Jump Action", comp.config.action.jump);
            DrawActionCombo("Targeting Action", comp.config.action.Targeting);
            DrawActionCombo("decide Action", comp.config.action.decide);
            DrawActionCombo("LockOn Action", comp.config.action.lockOn);
            DrawActionCombo("LockOnChenge Action", comp.config.action.lockOnChenge);

        }
        else {
            ImGui::Text("NO Input APIData)");
        }


        // 2. コンボボックスの制御（キャッシュ方式）
        static bool s_wasOpened = false;
        static std::vector<std::pair<unsigned long long, std::string>> s_entityCache;


        // --- 3. デバッグ用：隠れ場所への強制アタッチ (キャッシュ利用版) ---
        ImGui::SeparatorText("Debug: Force Hide Test");

        // staticキャッシュ変数の定義
        struct EntityCache { CCL::ECS::EntityID id; std::string name; };
        static std::vector<EntityCache> s_hideSpotCache;
        static bool s_cacheInitialized = false;

        // インスペクターのこのセクションが表示された際にキャッシュをクリア・再構築するボタン
        if (ImGui::Button("Refresh Hiding Spots")) {
            s_cacheInitialized = false;
        }
        return changed;
    }
};

// コンポーネントの自動登録
REGISTER_COMPONENT(TPSPlayerComponent, "TPSPlayerComponent")