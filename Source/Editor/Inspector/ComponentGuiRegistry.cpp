#include "ComponentGuiRegistry.h"
#include "ECS/Core/CCL_World.h" // Worldへのアクセスに必要
#include "Game/Core/AllComponents.h"
#include <imgui.h>

#include "Engine/Graphics/Core/GpuResourceUtils.h" // テクスチャロード用
#include "Engine/Graphics/Core/Graphics.h"         // デバイス取得用
#include "Engine/Platform/Dialog.h"              // ファイル選択ダイアログ用

#include "Engine/Serialization/Meta/ComponentMetaImGui.h"

// JSONとファイル入出力
#include <fstream>
#include <json.hpp>
#include <filesystem>
using json = nlohmann::json;


// ============================================================================
// シングルトンの実体を絶対にこの .cpp 1箇所だけで生成させる
// ============================================================================
ComponentGuiRegistry &ComponentGuiRegistry::Instance()
{
    static ComponentGuiRegistry s;
    return s;
}

// -----------------------------------------------------------------------
// JSON変換ヘルパー
// -----------------------------------------------------------------------

// Config -> JSON
json ConfigToJson(const ParticleSystemConfig &cfg, const char *texPath, const char *rampPath)
{
    // ★安全対策ヘルパー (数値が壊れていたら 0.0f にする)
    auto SafeFloat = [](float val) { return std::isfinite(val) ? val : 0.0f; };

    try {
        std::string safeTexPath  = (texPath && texPath[0] != '\0') ? texPath : "";
        std::string safeRampPath = (rampPath && rampPath[0] != '\0') ? rampPath : "";

        return {
            // 基本設定
            {"max_particle_count", cfg.max_particle_count},
            {"texture_path", safeTexPath},
            {"color_ramp_path", safeRampPath},
            // カラーモード (0:単色, 1:ランプ)
            {"color_mode", cfg.color_mode},
            {"type", cfg.type},
            {"emission_mode", cfg.emission_mode},
            {"burst_count", cfg.burst_count},

            // 発生パラメータ (SafeFloatを通すとより安全です)
            {"spawn_delay_min", SafeFloat(cfg.spawn_delay.x)},
            {"spawn_delay_max", SafeFloat(cfg.spawn_delay.y)},
            {"lifespan_min", SafeFloat(cfg.lifespan.x)},
            {"lifespan_max", SafeFloat(cfg.lifespan.y)},
            {"speed_min", SafeFloat(cfg.emission_speed.x)},
            {"speed_max", SafeFloat(cfg.emission_speed.y)},
            {"gravity", SafeFloat(cfg.gravity)},

            // 範囲・角度
            {"radius_min", SafeFloat(cfg.emission_offset.x)},
            {"radius_max", SafeFloat(cfg.emission_offset.y)},
            {"angle_min", SafeFloat(cfg.emission_cone_angle.x)},
            {"angle_max", SafeFloat(cfg.emission_cone_angle.y)},

            // 見た目
            {"color",
                {SafeFloat(cfg.manual_color.x),
                    SafeFloat(cfg.manual_color.y),
                    SafeFloat(cfg.manual_color.z),
                    SafeFloat(cfg.manual_color.w)}},
            {"size_start", SafeFloat(cfg.emission_size.x)},
            {"size_end", SafeFloat(cfg.emission_size.y)},
            {"fade_in", SafeFloat(cfg.fade_duration.x)},
            {"fade_out", SafeFloat(cfg.fade_duration.y)},

            // 拡張機能
            {"noise_scale", SafeFloat(cfg.noise_scale)},
            {"noise_strength", SafeFloat(cfg.noise_strength)},
            {"uv_scroll_x", SafeFloat(cfg.uv_scroll_speed.x)},
            {"uv_scroll_y", SafeFloat(cfg.uv_scroll_speed.y)},
            {"velocity_stretch", SafeFloat(cfg.velocity_stretch)},
            {"anim_mode", cfg.sprite_anim_mode},
            {"grid_x", cfg.sprite_sheet_grid.x},
            {"grid_y", cfg.sprite_sheet_grid.y},
        };
    }
    catch (std::exception &e) {
        // エラー時はログを出して空のJSONを返す（クラッシュ回避）
        return json({});
    }
}

// JSON -> Config
void JsonToConfig(const json &j, GPUParticleComponent *p)
{
    auto &cfg = p->config;

    // 値があれば読み込む (なければデフォルト維持)
    if (j.contains("max_particle_count")) {
        int newMax = j["max_particle_count"];
        // 最大数が変わる場合はリサイズ処理を通す
        if (newMax != cfg.max_particle_count) {
            p->maxParticles        = newMax;
            cfg.max_particle_count = newMax;
            p->isInitialized       = false; // フラグを折る
        }
    }

    // テクスチャ
    if (j.contains("texture_path")) {
        std::string path = j["texture_path"];
        if (!path.empty()) {
            strcpy_s(p->texturePath, sizeof(p->texturePath), path.c_str());
            p->isInitialized = false; // フラグを折る
        }
    }

    // Color Ramp パスの読み込み
    if (j.contains("color_ramp_path")) {
        std::string path = j["color_ramp_path"];
        if (!path.empty()) {
            strcpy_s(p->colorRampPath, sizeof(p->colorRampPath), path.c_str());
            p->isInitialized = false; // フラグを折る
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
        auto c           = j["color"];
        cfg.manual_color = {c[0], c[1], c[2], c[3]};
    }
    if (j.contains("size_start")) cfg.emission_size.x = j["size_start"];
    if (j.contains("size_end")) cfg.emission_size.y = j["size_end"];
    if (j.contains("fade_in")) cfg.fade_duration.x = j["fade_in"];
    if (j.contains("fade_out")) cfg.fade_duration.y = j["fade_out"];

    if (j.contains("noise_scale")) cfg.noise_scale = j["noise_scale"];

    // 拡張機能
     if (j.contains("noise_strength")) cfg.noise_strength = j["noise_strength"];
     if (j.contains("uv_scroll_x")) cfg.uv_scroll_speed.x = j["uv_scroll_x"];
     if (j.contains("uv_scroll_y")) cfg.uv_scroll_speed.y = j["uv_scroll_y"];
     if (j.contains("velocity_stretch")) cfg.velocity_stretch = j["velocity_stretch"];
     if (j.contains("anim_mode")) cfg.sprite_anim_mode = j["anim_mode"];
     if (j.contains("grid_x")) cfg.sprite_sheet_grid.x = j["grid_x"];
     if (j.contains("grid_y")) cfg.sprite_sheet_grid.y = j["grid_y"];
}


// Draw関数にも EntityID と World* を追加して渡す
void ComponentGuiRegistry::Draw(
    TypeID id, void* data, EntityID entity, CCL::ECS::Core::World* world)
{
    ResolveBindings(); // ★追加: 描画する直前に、IDマップが未完成なら完成させる

    auto idIt = _typeIdToIndex.find(id);
    if (idIt == _typeIdToIndex.end()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No GUI registered for this ID.");
        return;
    }
    auto it = _registry.find(idIt->second);
    if (it != _registry.end()) {
        it->second.guiFunc(data, entity, world);
    }
}