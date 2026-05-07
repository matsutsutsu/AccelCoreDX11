#include "ECS/Core/CCL_World.h"
#include "Engine/GamePlay/Graphics/Core/MaterialComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Dialog.h"

#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

#include "Engine/Core/Math/StringHash.h"


template <> struct ComponentMeta<MaterialComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Material";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(MaterialComponent& compRef, unsigned long long entityID = 0, void* worldPtr = nullptr)
    {
        bool               changed = false;
        MaterialComponent* comp = &compRef;
        auto* world = static_cast<CCL::ECS::Core::World*>(worldPtr);

        if (comp->overrideMaterials.empty()) {
            ImGui::TextDisabled("Using Shared Materials (Default)");
            bool hasModel = false;
            if (world) {
                auto* modelComp =
                    world->GetComponent<ModelComponent>(static_cast<CCL::ECS::EntityID>(entityID));
                if (modelComp && modelComp->GetModel()) {
                    hasModel = true;
                    if (ImGui::Button("Override All Materials (Edit)")) {
                        const auto& meshes = modelComp->GetModel()->GetMeshes();
                        comp->overrideMaterials.resize(meshes.size());
                        for (size_t i = 0; i < meshes.size(); ++i) {
                            auto* originalMat = meshes[i].material;
                            MaterialHandle newHandle = ResourceManager::Instance().CreateMaterial();
                            MaterialData* newData =
                                ResourceManager::Instance().GetMaterial(newHandle);
                            if (originalMat && originalMat->data && newData) {
                                *newData = *originalMat->data;
                                newData->name += " (Instance)";
                            }
                            comp->SetData(i, newHandle);
                        }
                        changed = true;
                    }
                }
            }
            if (!hasModel && ImGui::Button("Create Empty Material List")) {
                MaterialHandle emptyHandle = ResourceManager::Instance().CreateMaterial();
                comp->overrideMaterials.resize(1, emptyHandle);
                changed = true;
            }
            return changed;
        }

        if (ImGui::Button("Reset to Default (Discard Changes)")) {
            for (auto handle : comp->overrideMaterials) {
                ResourceManager::Instance().UnloadMaterial(handle);
            }
            comp->overrideMaterials.clear();
            return true;
        }

        // ====================================================================
        //  一括変更（バッチオペレーション）セクション
        // ====================================================================
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Batch Operations (Apply to All)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* shaderNames[] = { "Basic", "Lambert", "Phong", "Toon", "Outline", "ShadowMap", "PBR" };
            static int batchShaderIdx = 6; // デフォルトをPBRにしておく等の工夫

            // 少し幅を調整してボタンと並べる
            ImGui::SetNextItemWidth(150.0f);
            ImGui::Combo("##BatchShader", &batchShaderIdx, shaderNames, 7);
            ImGui::SameLine();

            // 一括適用ボタン
            if (ImGui::Button("Apply Shader to All Slots")) {
                uint32_t targetHash = CCL::Utils::HashString(shaderNames[batchShaderIdx]);
                for (size_t i = 0; i < comp->overrideMaterials.size(); ++i) {
                    MaterialHandle handle = comp->overrideMaterials[i];
                    if (!handle.IsValid()) continue;
                    MaterialData* matPtr = ResourceManager::Instance().GetMaterial(handle);
                    if (matPtr) {
                        matPtr->shaderHash = targetHash;
                    }
                }
                changed = true;
            }
        }

        ImGui::Separator();
        ImGui::Text("Material Slots:");

        for (size_t i = 0; i < comp->overrideMaterials.size(); ++i) {
            MaterialHandle matHandle = comp->overrideMaterials[i];
            if (!matHandle.IsValid()) continue;
            MaterialData* matPtr = ResourceManager::Instance().GetMaterial(matHandle);
            if (!matPtr) continue;

            ImGui::PushID((int)i);
            if (ImGui::TreeNode(matPtr->name.c_str())) {
                if (matPtr->colors.count("materialColor")) {
                    changed |= ImGui::ColorEdit4("Main Color", &matPtr->colors["materialColor"].x);
                }
                else if (ImGui::Button("Add Color Property")) {
                    matPtr->colors["materialColor"] = { 1, 1, 1, 1 };
                    changed = true;
                }

                // 修正後：文字列ハッシュのリストから選択させる
                const char* shaderNames[] = { "Basic", "Lambert", "Phong", "Toon", "Outline", "ShadowMap", "PBR" };
                int currentShaderIdx = 0;
                // 現在のハッシュ値からインデックスを逆引き
                for (int i = 0; i < 7; ++i) {
                    if (matPtr->shaderHash == CCL::Utils::HashString(shaderNames[i])) {
                        currentShaderIdx = i;
                        break;
                    }
                }
                if (ImGui::Combo("Shader", &currentShaderIdx, shaderNames, 7)) {
                    matPtr->shaderHash = CCL::Utils::HashString(shaderNames[currentShaderIdx]);
                    changed = true;
                }

                if (ImGui::TreeNode("Textures")) {
                    const char* slotNames[] = {
                        "DiffuseMap", "NormalMap", "EmissiveMap", "RampTexture" };
                    for (const char* slotName : slotNames) {
                        bool hasTexture =
                            (matPtr->textures.find(slotName) != matPtr->textures.end());
                        ImGui::PushID(slotName);
                        ImGui::Text("%s: %s", slotName, hasTexture ? "[Set]" : "[Empty]");
                        ImGui::SameLine();
                        if (ImGui::Button("Load...")) {
                            char        filename[256] = {};
                            const char* filter =
                                "Image Files\0*.png;*.jpg;*.tga;*.bmp\0All Files\0*.*\0";
                            if (Dialog::OpenFileName(filename,
                                256,
                                filter,
                                "Select Texture",
                                Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
                                if (SUCCEEDED(GpuResourceUtils::LoadTexture(
                                    Graphics::Instance().GetDevice(),
                                    filename,
                                    srv.GetAddressOf()))) {
                                    matPtr->textures[slotName] =
                                        ResourceManager::Instance().RegisterTexture(srv);
                                    changed = true;
                                }
                            }
                        }
                        if (!hasTexture && std::string(slotName) == "DiffuseMap") {
                            ImGui::SameLine();
                            if (ImGui::Button("Set White")) {
                                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteTex;
                                GpuResourceUtils::CreateDummyTexture(
                                    Graphics::Instance().GetDevice(),
                                    0xFFFFFFFF,
                                    whiteTex.GetAddressOf());
                                matPtr->textures[slotName] =
                                    ResourceManager::Instance().RegisterTexture(whiteTex);
                                changed = true;
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Advanced Parameters")) {
                    for (auto& [name, val] : matPtr->scalars) {
                        changed |= ImGui::DragFloat(name.c_str(), &val, 0.01f);
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> empty;
        return empty;
    }
};

// ============================================================================
// 究極の自動化：これだけでJSONとImGuiの両方に登録される
// ============================================================================
REGISTER_COMPONENT(MaterialComponent, "Material")