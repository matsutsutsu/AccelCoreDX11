#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Dialog.h"
#include <filesystem>

#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// ★追加: 逆引き処理のためにResourceManagerをインクルード
#include "Engine/Graphics/Resource/ResourceManager.h" 

// ============================================================================
// ModelComponent 専用の JSON シリアライズ処理
// ============================================================================
namespace ComponentMetaJson {
    // 保存（Save）時の処理
    template <>
    inline void Serialize<ModelComponent>(json& j, const ModelComponent& comp)
    {
        // 修正: コンポーネントは文字列を持たないため、Managerからハッシュを元に逆引きして保存する
        std::string path = ResourceManager::Instance().GetAssetPath(comp.assetHash);
        j["assetPath"] = path;
    }

    // 読込（Load）時の処理
    template <>
    inline void Deserialize<ModelComponent>(const json& j, ModelComponent& comp)
    {
        if (j.contains("assetPath") && j["assetPath"].is_string()) {
            std::string path = j["assetPath"].get<std::string>();
            // 修正: SetModel は const char* を受け取るように変更されたため、.c_str() を渡す
            comp.SetModel(path.c_str());
        }
    }
}

// ============================================================================
// Component Meta の定義
// ============================================================================
template <> struct ComponentMeta<ModelComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Model";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(ModelComponent& comp, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;

        // 修正: 表示用に現在のパスをManagerから取得
        std::string currentPath = ResourceManager::Instance().GetAssetPath(comp.assetHash);

        // 現在のモデルパスの表示
        ImGui::TextDisabled("Current Model:");
        if (currentPath.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(None / Empty)");
        }
        else {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", currentPath.c_str());
        }

        ImGui::Separator();

        // 実行中の動的ロードボタン
        if (ImGui::Button("Load New Model... (Dynamic)", ImVec2(-1, 0))) {
            char filename[256] = {};
            const char* filter = "Model Files\0*.gltf;*.glb;*.fbx;*.obj\0All Files\0*.*\0";

            if (Dialog::OpenFileName(filename, 256, filter, "Select 3D Model", Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {

                namespace fs = std::filesystem;
                fs::path absPath = filename;
                fs::path currentPathFs = fs::current_path();
                std::error_code ec;
                fs::path relPath = fs::relative(absPath, currentPathFs, ec);

                std::string finalPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;

                // モデルを切り替え（.c_str() で渡す）
                comp.SetModel(finalPath.c_str());
                changed = true;
            }
        }

        // モデルのクリアボタン
        if (!currentPath.empty()) {
            if (ImGui::Button("Clear Model (Remove)", ImVec2(-1, 0))) {
                comp.SetModel(""); // 空文字を渡すことで解放される
                changed = true;
            }
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
// 自動登録
// ============================================================================
REGISTER_COMPONENT(ModelComponent, "Model")