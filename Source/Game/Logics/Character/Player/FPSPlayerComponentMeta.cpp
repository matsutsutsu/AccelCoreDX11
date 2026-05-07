#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// プレイヤーコンポーネントと入力API
#include "FPSPlayerComponent.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "Engine/Platform/Input/InputFacade.h" // GetAxisNames等を使用する場合
#include "ECS/Core/CCL_World.h"

#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "PlayerTag.h"
#include "Game/Logic/System/HidingComponents.h"
#include "Engine/GamePlay/Core/NameComponent.h" // NameComponentの定義ヘッダー

// ============================================================================
// FPS Player Component Meta
// ============================================================================
template <> struct ComponentMeta<FPSPlayerComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "FPS Player Controller";
    static constexpr bool        hasCustomGui = true;

    // JSONシリアライズ対象のフィールド定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // 入力設定（名前を保存）
            META_FIELD_STRING(FPSPlayerComponent, config.axis.moveFB, "moveFB", "Move FB Action", "Input Settings"),
            META_FIELD_STRING(FPSPlayerComponent, config.axis.moveLR, "moveLR", "Move LR Action", "Input Settings"),
            META_FIELD_STRING(FPSPlayerComponent, config.action.dash, "dashKey", "Dash Action", "Input Settings"),

            META_FIELD_FLOAT(FPSPlayerComponent, walkSpeed, "walkSpeed", "Walk Speed", 0.1f, 0.0f, 20.0f, "Movement"),
            META_FIELD_FLOAT(FPSPlayerComponent, runSpeed, "runSpeed", "Run Speed", 0.1f, 0.0f, 40.0f, "Movement"),
            META_FIELD_FLOAT(FPSPlayerComponent, acceleration, "acceleration", "Acceleration", 0.01f, 0.0f, 1.0f, "Movement"),
        };
        return fields;
    }

    // インスペクター上でのカスタム描画（ドロップダウン選択）
    static bool CustomGui(FPSPlayerComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;
        auto* worldPtr = static_cast<CCL::ECS::Core::World*>(world);

        ImGui::InputFloat("currentSpeed", &comp.currentSpeed);
        ImGui::InputFloat("moveForward", &comp.input.moveForward);
        ImGui::InputFloat("moveRight", &comp.input.moveRight);

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
            DrawActionCombo("Dash Action", comp.config.action.dash);
            DrawActionCombo("Hide Action", comp.config.action.hide);

        }
        else {
            ImGui::Text("NO Input APIData)");
        }

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

        if (!s_cacheInitialized) {
            s_hideSpotCache.clear();
            // HidingSpotComponent を持っているエンティティを検索
            auto entities = worldPtr->View<HidingSpotComponent>();
                for (auto id : entities)
                {
                    if (id == entityID) continue;
                    auto* nameComp = worldPtr->GetComponent<NameComponent>(id);
                    std::string n = nameComp ? nameComp->name : "ID: " + std::to_string(id);
                    s_hideSpotCache.push_back({ id, n });
                }
            s_cacheInitialized = true;
        }

        // 状態チェック
        bool isAlreadyHiding = worldPtr->HasComponent<PlayerTag::HideTag>((CCL::ECS::EntityID)entityID);

        if (isAlreadyHiding) {
            if (ImGui::Button("Debug: Force Exit (Release Spot)")) {
                auto tag = worldPtr->GetComponent<PlayerTag::HideTag>((CCL::ECS::EntityID)entityID);
                // 隠れ場所側のフラグも念のため解放
                if (worldPtr->HasComponent<HidingSpotComponent>(tag->spotEntity)) {
                }
                worldPtr->RequestRemoveComponent<PlayerTag::HideTag>((CCL::ECS::EntityID)entityID);
                changed = true;
            }
        }
        else {
            if (s_hideSpotCache.empty()) {
                ImGui::TextDisabled("No HidingSpots found in world.");
            }
            else {
                static int selectedIdx = 0;
                if (selectedIdx >= s_hideSpotCache.size()) selectedIdx = 0;

                std::string comboLabel = s_hideSpotCache[selectedIdx].name;
                if (ImGui::BeginCombo("Target Spot", comboLabel.c_str())) {
                    for (int n = 0; n < s_hideSpotCache.size(); n++) {
                        bool isSelected = (selectedIdx == n);
                        if (ImGui::Selectable(s_hideSpotCache[n].name.c_str(), isSelected)) {
                            selectedIdx = n;
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Debug: Snap to HideSpot")) {
                    CCL::ECS::EntityID targetID = s_hideSpotCache[selectedIdx].id;
                    auto spot = worldPtr->GetComponent<HidingSpotComponent>(targetID);

                    // 1. HideTag を付与
                    PlayerTag::HideTag newTag;
                    newTag.spotEntity = targetID;
                    if (auto* t = worldPtr->GetComponent<TransformComponent>((CCL::ECS::EntityID)entityID)) {
                        newTag.originalPos = t->position;
                    }
                    worldPtr->AddComponent<PlayerTag::HideTag>((CCL::ECS::EntityID)entityID, newTag);

                    // 2. 物理的な接地フラグ等のリセットや、即座の座標更新が必要な場合はここで行う
                    spot->isOccupied = true;
                    changed = true;
                }
            }
        }

        return changed;
    }
};

// コンポーネントの自動登録
REGISTER_COMPONENT(FPSPlayerComponent, "FPSPlayerComponent")