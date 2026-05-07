#include "Engine/GamePlay/Graphics/Particle/GPUParticleComponent.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Dialog.h"
#include <filesystem>
#include <fstream>
#include <json.hpp>

#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// ❌ 削除: #include "Engine/Serialization/ComponentSerializers.h"

using json = nlohmann::json;

// ============================================================================
// ローカルヘルパー関数
// ============================================================================
namespace {
    json ConfigToJson(const ParticleSystemConfig& cfg, const char* texPath, const char* rampPath)
    {
        auto SafeFloat = [](float val) { return std::isfinite(val) ? val : 0.0f; };
        try {
            std::string safeTexPath = (texPath && texPath[0] != '\0') ? texPath : "";
            std::string safeRampPath = (rampPath && rampPath[0] != '\0') ? rampPath : "";
            return { {"max_particle_count", cfg.max_particle_count},
                {"texture_path", safeTexPath},
                {"color_ramp_path", safeRampPath},
                {"color_mode", cfg.color_mode},
                {"type", cfg.type},
                {"emission_mode", cfg.emission_mode},
                {"burst_count", cfg.burst_count},
                {"spawn_delay_min", SafeFloat(cfg.spawn_delay.x)},
                {"spawn_delay_max", SafeFloat(cfg.spawn_delay.y)},
                {"lifespan_min", SafeFloat(cfg.lifespan.x)},
                {"lifespan_max", SafeFloat(cfg.lifespan.y)},
                {"speed_min", SafeFloat(cfg.emission_speed.x)},
                {"speed_max", SafeFloat(cfg.emission_speed.y)},
                {"gravity", SafeFloat(cfg.gravity)},
                {"radius_min", SafeFloat(cfg.emission_offset.x)},
                {"radius_max", SafeFloat(cfg.emission_offset.y)},
                {"angle_min", SafeFloat(cfg.emission_cone_angle.x)},
                {"angle_max", SafeFloat(cfg.emission_cone_angle.y)},
                {"color",
                    {SafeFloat(cfg.manual_color.x),
                        SafeFloat(cfg.manual_color.y),
                        SafeFloat(cfg.manual_color.z),
                        SafeFloat(cfg.manual_color.w)}},
                {"size_start", SafeFloat(cfg.emission_size.x)},
                {"size_end", SafeFloat(cfg.emission_size.y)},
                {"fade_in", SafeFloat(cfg.fade_duration.x)},
                {"fade_out", SafeFloat(cfg.fade_duration.y)},
                {"noise_scale", SafeFloat(cfg.noise_scale)},
                {"noise_strength", SafeFloat(cfg.noise_strength)},
                {"uv_scroll_x", SafeFloat(cfg.uv_scroll_speed.x)},
                {"uv_scroll_y", SafeFloat(cfg.uv_scroll_speed.y)},
                {"velocity_stretch", SafeFloat(cfg.velocity_stretch)},
                {"anim_mode", cfg.sprite_anim_mode},
                {"grid_x", cfg.sprite_sheet_grid.x},
                {"grid_y", cfg.sprite_sheet_grid.y} };
        }
        catch (...) {
            return json({});
        }
    }

    void JsonToConfig(const json& j, GPUParticleComponent* p)
    {
        auto& cfg = p->config;
        if (j.contains("max_particle_count")) {
            int newMax = j["max_particle_count"];
            if (newMax != cfg.max_particle_count) {
                p->maxParticles = newMax;
                cfg.max_particle_count = newMax;
                p->isInitialized = false;
            }
        }
        if (j.contains("texture_path")) {
            std::string path = j["texture_path"];
            if (!path.empty()) {
                strcpy_s(p->texturePath, sizeof(p->texturePath), path.c_str());
                p->isInitialized = false;
            }
        }
        if (j.contains("color_ramp_path")) {
            std::string path = j["color_ramp_path"];
            if (!path.empty()) {
                strcpy_s(p->colorRampPath, sizeof(p->colorRampPath), path.c_str());
                p->isInitialized = false;
            }
        }
        if (j.contains("color_mode")) cfg.color_mode = j["color_mode"];
        if (j.contains("type")) cfg.type = j["type"];
        if (j.contains("emission_mode")) cfg.emission_mode = j["emission_mode"];
        if (j.contains("burst_count")) cfg.burst_count = j["burst_count"];
        if (j.contains("spawn_delay_min")) cfg.spawn_delay.x = j["spawn_delay_min"];
        if (j.contains("spawn_delay_max")) cfg.spawn_delay.y = j["spawn_delay_max"];
        if (j.contains("lifespan_min")) cfg.lifespan.x = j["lifespan_min"];
        if (j.contains("lifespan_max")) cfg.lifespan.y = j["lifespan_max"];
        if (j.contains("speed_min")) cfg.emission_speed.x = j["speed_min"];
        if (j.contains("speed_max")) cfg.emission_speed.y = j["speed_max"];
        if (j.contains("gravity")) cfg.gravity = j["gravity"];
        if (j.contains("radius_min")) cfg.emission_offset.x = j["radius_min"];
        if (j.contains("radius_max")) cfg.emission_offset.y = j["radius_max"];
        if (j.contains("angle_min")) cfg.emission_cone_angle.x = j["angle_min"];
        if (j.contains("angle_max")) cfg.emission_cone_angle.y = j["angle_max"];
        if (j.contains("color")) {
            auto c = j["color"];
            cfg.manual_color = { c[0], c[1], c[2], c[3] };
        }
        if (j.contains("size_start")) cfg.emission_size.x = j["size_start"];
        if (j.contains("size_end")) cfg.emission_size.y = j["size_end"];
        if (j.contains("fade_in")) cfg.fade_duration.x = j["fade_in"];
        if (j.contains("fade_out")) cfg.fade_duration.y = j["fade_out"];
        if (j.contains("noise_scale")) cfg.noise_scale = j["noise_scale"];
        if (j.contains("noise_strength")) cfg.noise_strength = j["noise_strength"];
        if (j.contains("uv_scroll_x")) cfg.uv_scroll_speed.x = j["uv_scroll_x"];
        if (j.contains("uv_scroll_y")) cfg.uv_scroll_speed.y = j["uv_scroll_y"];
        if (j.contains("velocity_stretch")) cfg.velocity_stretch = j["velocity_stretch"];
        if (j.contains("anim_mode")) cfg.sprite_anim_mode = j["anim_mode"];
        if (j.contains("grid_x")) cfg.sprite_sheet_grid.x = j["grid_x"];
        if (j.contains("grid_y")) cfg.sprite_sheet_grid.y = j["grid_y"];
    }
} // namespace

// ============================================================================
// Component Meta
// ============================================================================
template <> struct ComponentMeta<GPUParticleComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "GPU Particle";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(GPUParticleComponent& compRef, unsigned long long /*entityID*/ = 0, void* /*worldPtr*/ = nullptr)
    {
        bool changed = false;
        GPUParticleComponent* p = &compRef;
        auto& cfg = p->config;

        // 1. 管理・システム設定
        if (ImGui::Button(u8"プリセット保存 (Save Preset)")) {
            char filename[256] = {};
            if (Dialog::SaveFileName(filename,
                256,
                "JSON Files\0*.json\0",
                "Save Particle Preset",
                "json",
                Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                std::ofstream o(filename);
                o << std::setw(4) << ConfigToJson(cfg, p->texturePath, p->colorRampPath)
                    << std::endl;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"プリセット読込 (Load Preset)")) {
            char filename[256] = {};
            if (Dialog::OpenFileName(filename,
                256,
                "JSON Files\0*.json\0",
                "Load Particle Preset",
                Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                std::ifstream i(filename);
                if (i.is_open()) {
                    json j;
                    i >> j;
                    JsonToConfig(j, p);
                    cfg.burst_trigger = 0.0f;
                    p->maxParticles = cfg.max_particle_count;
                    p->isInitialized = false;
                    changed = true;
                }
            }
        }

        ImGui::Separator();

        // システム設定
        ImGui::TextDisabled(u8"システム設定 (System)");
        ImGui::Text(u8"稼働数 (Active): %d", cfg.max_particle_count);

        int tempMax = cfg.max_particle_count;
        if (ImGui::InputInt(u8"最大許容量 (Max Capacity)", &tempMax, 0, 0)) {
            if (tempMax > 0) {
                p->maxParticles = tempMax;
                p->isInitialized = false;
                changed = true;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(u8"Enterキーで適用します。\n注意: パーティクルが全てリセットされます。");
        }

        const char* typeNames[] = { "Point (Color Only)", "Quad (Texture)" };
        if (ImGui::Combo(u8"描画タイプ (Render Type)", &cfg.type, typeNames, IM_ARRAYSIZE(typeNames)))
            changed = true;
        ImGui::Spacing();

        // 2. 発生設定 (Emission)
        if (ImGui::TreeNode(u8"発生設定 (Emission)")) {
            const char* modes[] = { "Continuous (継続)", "Burst (爆発)" };
            if (ImGui::Combo(u8"発生モード (Mode)", &cfg.emission_mode, modes, IM_ARRAYSIZE(modes)))
                changed = true;

            if (cfg.emission_mode == 1) {
                if (ImGui::DragInt(u8"発生数 (Count)", &cfg.burst_count, 1, 0, 10000)) changed = true;
                if (ImGui::Button(u8"発火テスト (Trigger)", ImVec2(-1, 0))) {
                    p->TriggerBurst();
                    changed = true;
                }
            }
            else {
                if (ImGui::DragFloat2(
                    u8"発生間隔 (Delay Min/Max)", &cfg.spawn_delay.x, 0.001f, 0.0f, 1.0f, "%.4f"))
                    changed = true;
            }

            ImGui::Separator();
            ImGui::TextDisabled(u8"形状と位置 (Shape & Position)");
            if (ImGui::DragFloat4(u8"位置オフセット (Pos Offset)", &cfg.emission_position.x, 0.1f))
                changed = true;
            if (ImGui::DragFloat2(u8"半径 (Radius Min/Max)", &cfg.emission_offset.x, 0.1f))
                changed = true;
            if (ImGui::DragFloat(
                u8"形状比率 X (Shape Stretch X)", &cfg.emission_stretch_x, 0.01f, 0.0f, 10.0f))
                changed = true;
            if (ImGui::DragFloat(
                u8"形状比率 Z (Shape Stretch Z)", &cfg.emission_stretch_z, 0.01f, 0.0f, 10.0f))
                changed = true;
            if (ImGui::DragFloat(u8"高さ範囲 Y (Height Volume)", &cfg.particle_offset_y, 0.1f, 0.0f, 50.0f))
                changed = true;
            if (ImGui::DragFloat2(u8"角度 (Angle Min/Max)", &cfg.emission_cone_angle.x, 0.1f))
                changed = true;
            ImGui::TreePop();
        }

        // 3. 挙動・物理設定 (Movement & Physics)
        if (ImGui::TreeNode(u8"挙動・物理 (Movement)")) {
            if (ImGui::DragFloat(u8"重力 (Gravity)", &cfg.gravity, 0.1f)) changed = true;
            if (ImGui::DragFloat2(u8"速度 (Speed Min/Max)", &cfg.emission_speed.x, 0.1f))
                changed = true;
            if (ImGui::DragFloat2(u8"回転速度 (Rot Speed Min/Max)", &cfg.emission_angular_speed.x, 0.1f))
                changed = true;
            if (ImGui::DragFloat2(u8"寿命 (Life Min/Max)", &cfg.lifespan.x, 0.1f)) changed = true;

            ImGui::Separator();
            ImGui::TextDisabled(u8"乱流・ノイズ (Turbulence)");
            if (ImGui::DragFloat(u8"ノイズ強度 (Strength)", &cfg.noise_strength, 0.1f))
                changed = true;
            if (ImGui::DragFloat(u8"ノイズサイズ (Scale)", &cfg.noise_scale, 0.01f)) changed = true;
            ImGui::TreePop();
        }

        // 4. 見た目・アニメーション (Appearance)
        if (ImGui::TreeNode(u8"見た目 (Appearance)")) {
            const char* renderModes[] = { "Additive (Fire/Light)", "Alpha Blend (Smoke/Dark)" };
            int         currentMode = p->config.render_mode;
            if (ImGui::Combo(
                "Render Mode", &currentMode, renderModes, IM_ARRAYSIZE(renderModes))) {
                p->config.render_mode = currentMode;
                changed = true;
            }

            if (ImGui::ColorEdit4(u8"基本色 (Base Color)", &cfg.manual_color.x)) changed = true;

            const char* colorModes[] = { u8"単色 (Single Color)", u8"カラーランプ (Color Ramp)" };
            if (ImGui::Combo(
                u8"カラーモード (Color Mode)", &cfg.color_mode, colorModes, IM_ARRAYSIZE(colorModes)))
                changed = true;

            if (cfg.color_mode == 1) {
                ImGui::TextDisabled(u8"※テクスチャ項目の Color Ramp が使用されます");
            }

            if (ImGui::DragFloat2(u8"サイズ (Size Start/End)", &cfg.emission_size.x, 0.1f))
                changed = true;
            if (ImGui::DragFloat2(u8"フェード (Fade In/Out)", &cfg.fade_duration.x, 0.01f))
                changed = true;

            ImGui::Separator();
            ImGui::TextDisabled(u8"特殊効果 (Effects)");
            if (ImGui::DragFloat(
                u8"速度ストレッチ (Stretch)", &cfg.velocity_stretch, 0.01f, 0.0f, 10.0f))
                changed = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"移動方向に引き伸ばします (0.0=無効)");
            if (ImGui::DragFloat2(u8"UVスクロール (UV Scroll)", &cfg.uv_scroll_speed.x, 0.01f))
                changed = true;

            ImGui::Separator();
            ImGui::TextDisabled(u8"スプライトアニメ (Sprite Sheet)");
            int grid[2] = { (int)cfg.sprite_sheet_grid.x, (int)cfg.sprite_sheet_grid.y };
            if (ImGui::DragInt2(u8"分割数 (Grid X/Y)", grid, 0.1f, 1, 16)) {
                cfg.sprite_sheet_grid.x = grid[0];
                cfg.sprite_sheet_grid.y = grid[1];
                changed = true;
            }

            const char* animModes[] = { "Random (Fixed)", "Life Cycle (Animate)" };
            if (ImGui::Combo(
                u8"再生モード (Anim Mode)", &cfg.sprite_anim_mode, animModes, IM_ARRAYSIZE(animModes)))
                changed = true;
            ImGui::TreePop();
        }

        // 5. テクスチャ設定 (Textures)
        if (ImGui::TreeNode(u8"テクスチャ (Textures)")) {
            // メインテクスチャ
            ImGui::TextDisabled(u8"メインテクスチャ (Main Texture)");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
            if (ImGui::InputText("##texPath", p->texturePath, sizeof(p->texturePath)))
                changed = true;
            ImGui::SameLine();

            if (ImGui::Button(u8"読込##Main")) {
                char        filename[256] = {};
                HWND        hWnd = Graphics::Instance().GetWindowHandle();
                const char* filter = "Image Files\0*.png;*.jpg;*.tga;*.bmp;*.dds\0All Files\0*.*\0";
                if (Dialog::OpenFileName(
                    filename, sizeof(filename), filter, "Select Texture", hWnd) == DialogResult::OK) {
                    namespace fs = std::filesystem;
                    fs::path absPath = filename;
                    fs::path currentPath = fs::current_path();
                    std::error_code ec;
                    fs::path relPath = fs::relative(absPath, currentPath, ec);
                    if (!ec && !relPath.empty()) {
                        strcpy_s(p->texturePath,
                            sizeof(p->texturePath),
                            relPath.generic_string().c_str());
                    }
                    else {
                        strcpy_s(p->texturePath, sizeof(p->texturePath), filename);
                    }
                    p->isInitialized = false;
                    changed = true;
                }
            }

            ID3D11ShaderResourceView* texSRV = ResourceManager::Instance().GetTexture(p->texture);
            if (texSRV) {
                ImGui::Image((void*)texSRV,
                    ImVec2(64, 64),
                    ImVec2(0, 0),
                    ImVec2(1, 1),
                    ImVec4(1, 1, 1, 1),
                    ImVec4(1, 1, 1, 0.5f));
                ImGui::SameLine();
                if (ImGui::Button(u8"クリア##Main")) {
                    memset(p->texturePath, 0, sizeof(p->texturePath));
                    p->texture = TextureHandle{};
                    changed = true;
                }
            }

            ImGui::Separator();

            // カラーランプ
            ImGui::TextDisabled(u8"カラーランプ (Color Ramp)");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
            if (ImGui::InputText("##rampPath", p->colorRampPath, sizeof(p->colorRampPath)))
                changed = true;
            ImGui::SameLine();

            if (ImGui::Button(u8"読込##Ramp")) {
                char        filename[256] = {};
                HWND        hWnd = Graphics::Instance().GetWindowHandle();
                const char* filter = "Image Files\0*.png;*.jpg;*.tga;*.bmp;*.dds\0All Files\0*.*\0";
                if (Dialog::OpenFileName(
                    filename, sizeof(filename), filter, "Select Color Ramp", hWnd) == DialogResult::OK) {
                    namespace fs = std::filesystem;
                    fs::path absPath = filename;
                    fs::path currentPath = fs::current_path();
                    std::error_code ec;
                    fs::path relPath = fs::relative(absPath, currentPath, ec);
                    if (!ec && !relPath.empty()) {
                        strcpy_s(p->colorRampPath,
                            sizeof(p->colorRampPath),
                            relPath.generic_string().c_str());
                    }
                    else {
                        strcpy_s(p->colorRampPath, sizeof(p->colorRampPath), filename);
                    }
                    p->isInitialized = false;
                    changed = true;
                }
            }

            ID3D11ShaderResourceView* rampSRV =
                ResourceManager::Instance().GetTexture(p->colorRamp);
            if (rampSRV) {
                ImGui::Image((void*)rampSRV,
                    ImVec2(128, 16),
                    ImVec2(0, 0),
                    ImVec2(1, 1),
                    ImVec4(1, 1, 1, 1),
                    ImVec4(1, 1, 1, 0.5f));
                ImGui::SameLine();
                if (ImGui::Button(u8"クリア##Ramp")) {
                    memset(p->colorRampPath, 0, sizeof(p->colorRampPath));
                    p->colorRamp = TextureHandle{};
                    changed = true;
                }
            }
            ImGui::TreePop();
        }

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        // GPUパーティクルは現状プリセットファイルで管理されているため、通常のシーンシリアライズには含めない設計にします。
        static const std::vector<FieldDescriptor> empty;
        return empty;
    }
};

// ============================================================================
// 究極の自動化
// ============================================================================
REGISTER_COMPONENT(GPUParticleComponent, "GPUParticle")