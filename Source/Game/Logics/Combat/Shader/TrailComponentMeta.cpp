#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "TrailComponent.h" // 適切なパスに修正してください
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Dialog.h"
#include "Engine/Graphics/Resource/ResourceManager.h"
#include <imgui.h>
#include <filesystem>

template <>
struct ComponentMeta<TrailComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Trail";

    // ★ カスタムGUIを有効化！
    static constexpr bool hasCustomGui = true;

    // JSONの保存/読み込みは自動化基盤（Fields）に任せる
    static constexpr bool isSerializable = true;
    static constexpr bool hasCustomSerialize = false;

    // ========================================================================
    // カスタムGUIの実装
    // ========================================================================
    static bool CustomGui(TrailComponent& comp, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr) {
        bool changed = false;

        // 1. 状態
        if (ImGui::Checkbox(u8"エミット中 (Is Emitting)", &comp.isEmitting)) changed = true;

        // 2. パラメータ
        if (ImGui::TreeNodeEx(u8"パラメータ (Parameters)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::DragFloat(u8"寿命 (Life Time)", &comp.lifeTime, 0.01f, 0.0f, 10.0f)) changed = true;
            if (ImGui::DragFloat(u8"頂点間隔 (Min Vertex Distance)", &comp.minVertexDistance, 0.01f, 0.0f, 2.0f)) changed = true;
            if (ImGui::ColorEdit4(u8"色 (Color)", &comp.color.x)) changed = true;
            ImGui::TreePop();
        }

        // 3. ローカル座標
        if (ImGui::TreeNodeEx(u8"形状 (Transform)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::DragFloat3(u8"根本の座標 (Local Base Pos)", &comp.localBasePos.x, 0.05f)) changed = true;
            if (ImGui::DragFloat3(u8"先端の座標 (Local Tip Pos)", &comp.localTipPos.x, 0.05f)) changed = true;
            ImGui::TreePop();
        }

        // 4. テクスチャ選択（パーティクル仕様の高機能UI）
        if (ImGui::TreeNodeEx(u8"テクスチャ (Texture)", ImGuiTreeNodeFlags_DefaultOpen)) {

            // std::string を ImGui::InputText で扱うためのバッファ
            char buf[256];
            strcpy_s(buf, sizeof(buf), comp.texturePath.c_str());

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
            if (ImGui::InputText("##texPath", buf, sizeof(buf))) {
                comp.texturePath = buf;
                comp.textureHandle = TextureHandle{}; // パスが変わったらハンドルを破棄（再ロードを促す）
                changed = true;
            }
            ImGui::SameLine();

            // 読込ボタン（ダイアログ）
            if (ImGui::Button(u8"読込##TrailTex")) {
                char filename[MAX_PATH] = {};
                DialogConfig cfg;
                cfg.title = "Select Trail Texture";
                cfg.filter = "Image Files\0*.png;*.jpg;*.tga;*.dds\0All Files\0*.*\0";
                cfg.defaultDir = "Assets/Textures/Effect";
                cfg.historyKey = "TrailTexture";

                if (Dialog::OpenFileName(filename, MAX_PATH, cfg, Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                    namespace fs = std::filesystem;
                    fs::path absPath = filename;
                    fs::path currentPath = fs::current_path();
                    std::error_code ec;
                    fs::path relPath = fs::relative(absPath, currentPath, ec);

                    // プロジェクト内のファイルなら相対パスにする
                    if (!ec && !relPath.empty()) {
                        comp.texturePath = relPath.generic_string();
                    }
                    else {
                        comp.texturePath = filename;
                    }
                    comp.textureHandle = TextureHandle{}; // リセットして再ロードフラグを立てる
                    changed = true;
                }
            }

            // プレビュー表示とクリアボタン
            if (comp.textureHandle.IsValid()) {
                ID3D11ShaderResourceView* texSRV = ResourceManager::Instance().GetTexture(comp.textureHandle);
                if (texSRV) {
                    ImGui::Image((void*)texSRV,
                        ImVec2(64, 64),
                        ImVec2(0, 0),
                        ImVec2(1, 1),
                        ImVec4(1, 1, 1, 1),
                        ImVec4(1, 1, 1, 0.5f)); // 背景を少し暗くして加算を見やすくする

                    ImGui::SameLine();

                    if (ImGui::Button(u8"クリア##TrailTex")) {
                        comp.texturePath.clear();
                        comp.textureHandle = TextureHandle{};
                        changed = true;
                    }
                }
            }

            ImGui::TreePop();
        }

        return changed;
    }

    // ========================================================================
    // JSON保存用のフィールド定義 (hasCustomGui = true でも保存にはこれが使われる)
    // ========================================================================
    static const std::vector<FieldDescriptor>& Fields() {
        static std::vector<FieldDescriptor> fields = {
            META_FIELD_BOOL(TrailComponent, isEmitting, "isEmitting", "Is Emitting", "State"),
            META_FIELD_FLOAT(TrailComponent, lifeTime, "lifeTime", "Life Time", 0.01f, 0.0f, 10.0f, "Parameters"),
            META_FIELD_FLOAT(TrailComponent, minVertexDistance, "minVertexDistance", "Min Vertex Distance", 0.01f, 0.0f, 2.0f, "Parameters"),
            META_FIELD_FLOAT4(TrailComponent, color, "color", "Color (RGBA)", 0.01f, "Parameters"),
            META_FIELD_FLOAT3(TrailComponent, localBasePos, "localBasePos", "Local Base Pos", 0.05f, "Transform"),
            META_FIELD_FLOAT3(TrailComponent, localTipPos, "localTipPos", "Local Tip Pos", 0.05f, "Transform"),

            // パス情報だけをJSONに保存させる
            META_FIELD_STRING(TrailComponent, texturePath, "texturePath", "Texture Path", "Rendering")
        };
        return fields;
    }
};

// コンポーネント登録マクロ（プロジェクトのルールに従ってください）
REGISTER_COMPONENT(TrailComponent, "Trail")