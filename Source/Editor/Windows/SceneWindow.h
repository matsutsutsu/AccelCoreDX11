//#pragma once
//
//#include "Editor/Core/EditorWindow.h"
//
//
//#include "Game/Logics/Character/Player/PlayerComponent.h"
//#include "Game/Logics/Combat/Weapon/Core/GunComponent.h"
//
//#include "Editor/UIComponents/ComponentGuiRegistry.h"
//
//#include "Engine/Serialization/Factory/Prefab.h"
//
//class SceneWindow : public EditorWindow {
//public:
//    SceneWindow() : EditorWindow("Scene Window") {}
//protected:
//    void DrawContents(EditorContext &context) override {
//        CCL::ECS::Core::World *world = context.world;
//        ComponentGuiRegistry* gunRegistry = &context.guiRegistry;
//
//        if (auto view = world->View<TitleInputComponent>(); !view.empty()) {
//            ImGui::Text("Title Scene");
//            for (auto entity : view) {
//                auto input = world->GetComponent<TitleInputComponent>(entity);
//                ImGui::Checkbox(
//                    "Game Start | Controller : Press A or Keyboard : Press Z |", &input->isDecided);
//            }
//        }
//        else {
//            ImGui::Text("Game Scene");
//            auto playerView = world->View<PlayerComponent>();
//            if (playerView.empty()) {
//                if (ImGui::Button("Spawn Player")) Prefab::Spawn(*world, Prefab::ID::Player);
//            } else {
//                for (auto playerEntity : playerView) {
//                    auto  &chunkManager     = world->GetChunkManager();
//                    size_t entityIdxInChunk = 0;
//                    size_t chunkIdx = chunkManager.SearchEntityIn(playerEntity, &entityIdxInChunk);
//
//                    if (chunkIdx != InvalidIndex) {
//                        auto       &chunk     = chunkManager.GetChunks()[chunkIdx];
//                        const auto &archetype = chunk->GetArchetype();
//                        auto       &registry  = context.guiRegistry;
//
//                        // --- 既存コンポーネントの描画と削除機能 ---
//                        for (const auto &typeData : archetype) {
//                            void *compPtr =
//                                chunk->GetComponentPtrByType(typeData.id, entityIdxInChunk);
//                            std::string compName = registry.GetName(typeData.id);
//
//                            auto draw = [&](const std::string &name) {
//                                if (compName != name)
//                                    return;
//
//                                ImGui::PushID((int)typeData.id);
//                                bool headerOpen = ImGui::CollapsingHeader(name.c_str(),
//                                    ImGuiTreeNodeFlags_DefaultOpen |
//                                        ImGuiTreeNodeFlags_AllowItemOverlap);
//                                // ヘッダーの右側に削除ボタンを配置
//                                ImGui::SameLine(ImGui::GetWindowWidth() - 35);
//                                if (ImGui::Button("x", ImVec2(25, 18))) {
//                                    // コンポーネント削除リクエスト
//                                    world->RequestRemoveComponent(playerEntity, typeData.id);
//                                }
//                                if (headerOpen) {
//                                    ImGui::Indent();
//                                    registry.Draw(typeData.id, compPtr, playerEntity, world);
//                                    ImGui::Unindent();
//                                    ImGui::Spacing();
//                                }
//                                ImGui::PopID();
//                            };
//
//                            draw("Gun Build");
//                            draw("Health");
//                        }
//                    }
//                }
//            }
//        }
//    }
//};