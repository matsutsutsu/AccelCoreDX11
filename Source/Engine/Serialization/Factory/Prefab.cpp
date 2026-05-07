#include "Prefab.h"
#include "Engine/Serialization/SceneSerializer.h" // 汎用シリアライザ
#include "Game/Core/AllComponents.h"
#include <SimpleMath.h>

#include "Engine/GamePlay/Transform/TransformUpdateSystem.h"

// インクルード
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include <windows.h>
#include <filesystem> // ファイル存在確認用
#include <sstream> // エラーメッセージ構築用

using namespace DirectX::SimpleMath;

// ----------------------------------------------------------------------------
// 🌟 JSON ADL ヘルパー
// ParticleSystemConfig と同じ名前空間（通常はグローバル）に配置することで、
// nlohmann::json の関数が自動的にこれらを発見します。
// ----------------------------------------------------------------------------
void to_json(nlohmann::json &j, const ParticleSystemConfig &c)
{
    auto SafeFloat = [](float val) { return std::isfinite(val) ? val : 0.0f; };

    j = nlohmann::json{{"max_particle_count", c.max_particle_count},
        {"type", c.type},
        {"color_mode", c.color_mode},
        {"emission_mode", c.emission_mode},
        {"burst_count", c.burst_count},
        {"gravity", SafeFloat(c.gravity)},
        {"velocity_stretch", SafeFloat(c.velocity_stretch)},
        {"noise_scale", SafeFloat(c.noise_scale)},
        {"noise_strength", SafeFloat(c.noise_strength)},
        {"particle_offset_y", SafeFloat(c.particle_offset_y)},
        {"emission_stretch_x", SafeFloat(c.emission_stretch_x)},
        {"emission_stretch_z", SafeFloat(c.emission_stretch_z)},
        {"sprite_anim_mode", c.sprite_anim_mode}};

    j["spawn_delay"]            = {SafeFloat(c.spawn_delay.x), SafeFloat(c.spawn_delay.y)};
    j["lifespan"]               = {SafeFloat(c.lifespan.x), SafeFloat(c.lifespan.y)};
    j["emission_speed"]         = {SafeFloat(c.emission_speed.x), SafeFloat(c.emission_speed.y)};
    j["emission_angular_speed"] = {
        SafeFloat(c.emission_angular_speed.x), SafeFloat(c.emission_angular_speed.y)};
    j["emission_offset"]     = {SafeFloat(c.emission_offset.x), SafeFloat(c.emission_offset.y)};
    j["emission_cone_angle"] = {
        SafeFloat(c.emission_cone_angle.x), SafeFloat(c.emission_cone_angle.y)};

    j["manual_color"] = {SafeFloat(c.manual_color.x),
        SafeFloat(c.manual_color.y),
        SafeFloat(c.manual_color.z),
        SafeFloat(c.manual_color.w)};

    j["emission_size"]     = {SafeFloat(c.emission_size.x), SafeFloat(c.emission_size.y)};
    j["fade_duration"]     = {SafeFloat(c.fade_duration.x), SafeFloat(c.fade_duration.y)};
    j["uv_scroll_speed"]   = {SafeFloat(c.uv_scroll_speed.x), SafeFloat(c.uv_scroll_speed.y)};
    j["sprite_sheet_grid"] = {c.sprite_sheet_grid.x, c.sprite_sheet_grid.y};

    // もし particle_scale が XMFLOAT3 などである場合を想定して to_json にも追加
    j["particle_scale"] = {SafeFloat(c.particle_scale.x), SafeFloat(c.particle_scale.y)};
}

void from_json(const nlohmann::json &j, ParticleSystemConfig &c)
{
    // ★ 安全に配列から値を読み取るためのローカルヘルパーラムダ関数
    // これにより get_to() のコンパイルエラーを完全に回避します
    auto ReadFloat2 = [&](const char *key, auto &vec) {
        if (j.contains(key) && j[key].is_array() && j[key].size() >= 2) {
            vec.x = j[key][0];
            vec.y = j[key][1];
        }
    };

    auto ReadFloat4 = [&](const char *key, auto &vec) {
        if (j.contains(key) && j[key].is_array() && j[key].size() >= 4) {
            vec.x = j[key][0];
            vec.y = j[key][1];
            vec.z = j[key][2];
            vec.w = j[key][3];
        }
    };

    // --- Vector系の読み込み (ヘルパーを使用して安全に代入) ---
    ReadFloat2("emission_speed", c.emission_speed);
    ReadFloat2("emission_angular_speed", c.emission_angular_speed);
    ReadFloat2("emission_offset", c.emission_offset);
    ReadFloat2("emission_cone_angle", c.emission_cone_angle);
    ReadFloat2("emission_size", c.emission_size);
    ReadFloat2("uv_scroll_speed", c.uv_scroll_speed);
    ReadFloat2("sprite_sheet_grid", c.sprite_sheet_grid);
    ReadFloat2("lifespan", c.lifespan);
    ReadFloat2("spawn_delay", c.spawn_delay);
    ReadFloat2("fade_duration", c.fade_duration);

    ReadFloat2("particle_scale", c.particle_scale);

    ReadFloat4("manual_color", c.manual_color);

    // --- 通常の値の読み込み ---
    if (j.contains("emission_mode")) c.emission_mode = j["emission_mode"];
    if (j.contains("burst_count")) c.burst_count = j["burst_count"];
    if (j.contains("color_mode")) c.color_mode = j["color_mode"];
    if (j.contains("max_particle_count")) c.max_particle_count = j["max_particle_count"];
    if (j.contains("type")) c.type = j["type"];

    if (j.contains("gravity")) c.gravity = j["gravity"];
    if (j.contains("velocity_stretch")) c.velocity_stretch = j["velocity_stretch"];
    if (j.contains("noise_scale")) c.noise_scale = j["noise_scale"];
    if (j.contains("noise_strength")) c.noise_strength = j["noise_strength"];
    if (j.contains("particle_offset_y")) c.particle_offset_y = j["particle_offset_y"];
    if (j.contains("emission_stretch_x")) c.emission_stretch_x = j["emission_stretch_x"];
    if (j.contains("emission_stretch_z")) c.emission_stretch_z = j["emission_stretch_z"];
    if (j.contains("sprite_anim_mode")) c.sprite_anim_mode = j["sprite_anim_mode"];
}

// ヘルパー: Shift-JIS(Windowsパス) -> UTF-8(JSON) ＆ 相対パス化
// (※もしこれも消したファイルにあり、Prefab.cppで使っているなら移植します)
namespace {
    std::string PathToRelativeUtf8(const std::string &pathSjis)
    {
        if (pathSjis.empty()) return "";
        int          len = MultiByteToWideChar(CP_ACP, 0, pathSjis.c_str(), -1, NULL, 0);
        std::wstring wstr(len, 0);
        MultiByteToWideChar(CP_ACP, 0, pathSjis.c_str(), -1, &wstr[0], len);
        wstr.resize(len - 1);

        namespace fs                = std::filesystem;
        fs::path        absPath     = wstr;
        fs::path        currentPath = fs::current_path();
        std::error_code ec;
        fs::path        relPath = fs::relative(absPath, currentPath, ec);
        std::wstring finalWstr  = (!ec && !relPath.empty()) ? relPath.wstring() : absPath.wstring();

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, finalWstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string utf8Str(utf8Len, 0);
        WideCharToMultiByte(CP_UTF8, 0, finalWstr.c_str(), -1, &utf8Str[0], utf8Len, NULL, NULL);
        utf8Str.resize(utf8Len - 1);
        std::replace(utf8Str.begin(), utf8Str.end(), '\\', '/');
        return utf8Str;
    }
} // namespace

namespace Prefab {

    // =========================================================
    // 内部構造体
    // =========================================================
    struct ParticleNode {
        ParticleSystemConfig config;
        std::string          texturePath;
        std::string          colorRampPath;
        DirectX::XMFLOAT3    offsetPosition = {0, 0, 0};
    };

    struct CompositeBlueprint {
        std::vector<ParticleNode> nodes;
    };

    namespace {
        ID3D11Device *g_device = nullptr;

        // パーティクル用キャッシュ
        std::unordered_map<std::string, CompositeBlueprint>    g_particleCache;
        std::unordered_map<std::string, std::vector<EntityID>> g_particlePool;
    } // namespace

    void Cleanup()
    {
        g_particlePool.clear();
        g_particleCache.clear();
        g_device = nullptr;
    }

    void Initialize(ID3D11Device *device)
    {
        g_device = device;
        // RegisterAllPrefabs はもう不要なので削除
    }

    // =========================================================
    // ヘルパー: 設計図の取得 (変更なし)
    // =========================================================
    const CompositeBlueprint &GetCachedCompositeBlueprint(const std::string &path)
    {
        auto it = g_particleCache.find(path);
        if (it != g_particleCache.end()) {
            return it->second;
        }

        CompositeBlueprint bp;
        std::ifstream      i(path);
        if (i.is_open()) {
            try {
                json root;
                i >> root;
                if (root.contains("entities") && root["entities"].is_array()) {
                    for (auto &entity : root["entities"]) {
                        if (entity.contains("components")) {
                            auto &comps = entity["components"];
                            if (comps.contains("GPUParticle")) {
                                ParticleNode node;
                                auto        &pData = comps["GPUParticle"];
                                if (pData.contains("config")) pData["config"].get_to(node.config);
                                if (pData.contains("maxParticles"))
                                    node.config.max_particle_count = pData["maxParticles"];
                                else if (node.config.max_particle_count <= 0)
                                    node.config.max_particle_count = 10000;
                                node.texturePath   = pData.value("texturePath", "");
                                node.colorRampPath = pData.value("colorRampPath", "");
                                if (comps.contains("Transform")) {
                                    auto &tData = comps["Transform"];
                                    if (tData.contains("position") &&
                                        tData["position"].is_array()) {
                                        node.offsetPosition.x = tData["position"][0];
                                        node.offsetPosition.y = tData["position"][1];
                                        node.offsetPosition.z = tData["position"][2];
                                    }
                                }
                                bp.nodes.push_back(node);
                            }
                        }
                    }
                }
            }
            catch (...) {
            }
        }
        g_particleCache[path] = bp;
        return g_particleCache[path];
    }

    // =====================================================================
    // プール返却 (変更なし)
    // =====================================================================
    void ReturnParticleToPool(Core::World &world, EntityID id)
    {
        auto *pooled = world.GetComponent<PooledParticleComponent>(id);
        if (!pooled) {
            world.Destroy(id);
            return;
        }
        pooled->isSleeping = true;

        if (auto *t = world.GetComponent<TransformComponent>(id)) {
            t->position = DirectX::XMFLOAT3(0, -9999, 0);
            TransformUpdateSystem::SetParent(world, id, 0);
        }
        if (auto *p = world.GetComponent<GPUParticleComponent>(id)) {
            p->config.time = 0.0f;
        }
        g_particlePool[pooled->poolKey].push_back(id);
    }

    // =====================================================================
    // パーティクル生成 (変更なしだが、プール再利用時のPending処理を追加済み)
    // =====================================================================
    EntityID SpawnParticle(Core::World &world,
        const std::string              &path,
        EntityID                        parentID,
        const DirectX::XMFLOAT3        &localPos)
    {
        const CompositeBlueprint &blueprint = GetCachedCompositeBlueprint(path);
        if (blueprint.nodes.empty()) return 0;

        EntityID lastID = 0;

        for (size_t i = 0; i < blueprint.nodes.size(); ++i) {
            const auto &node       = blueprint.nodes[i];
            std::string poolKey    = path + "#" + std::to_string(i);
            EntityID    particleID = 0;

            // A. プールから再利用
            if (!g_particlePool[poolKey].empty()) {
                particleID = g_particlePool[poolKey].back();
                g_particlePool[poolKey].pop_back();

                if (auto *pooled = world.GetComponent<PooledParticleComponent>(particleID)) {
                    pooled->isSleeping = false;
                }

                bool parentSetSuccess = false;
                if (auto *trans = world.GetComponent<TransformComponent>(particleID)) {
                    TransformUpdateSystem::SetParent(world, particleID, parentID);
                    if (parentID != 0 && trans->parentID == parentID) {
                        parentSetSuccess = true;
                    }
                    trans->position.x = localPos.x + node.offsetPosition.x;
                    trans->position.y = localPos.y + node.offsetPosition.y;
                    trans->position.z = localPos.z + node.offsetPosition.z;
                }



                if (parentID != 0 && !parentSetSuccess) {
                    if (!world.HasComponent<PendingParentComponent>(particleID)) {
                        PendingParentComponent pending;
                        pending.parentID = parentID;
                        world.AddComponent<PendingParentComponent>(particleID, pending);
                    }
                }

                if (auto *gpuP = world.GetComponent<GPUParticleComponent>(particleID)) {
                    gpuP->config = node.config;
                    if (!node.texturePath.empty())
                        strcpy_s(
                            gpuP->texturePath, sizeof(gpuP->texturePath), node.texturePath.c_str());
                    if (!node.colorRampPath.empty())
                        strcpy_s(gpuP->colorRampPath,
                            sizeof(gpuP->colorRampPath),
                            node.colorRampPath.c_str());
                    if (gpuP->maxParticles < node.config.max_particle_count) {
                        gpuP->maxParticles  = node.config.max_particle_count;
                        gpuP->isInitialized = false;
                    }
                }
            }
            // B. 新規作成
            else {
                auto arch = ArchetypeHelper::
                    Generate<TransformComponent, GPUParticleComponent, PooledParticleComponent>();
                particleID = world.RequestSpawnEntity(arch);

                TransformComponent trans;
                trans.position.x = localPos.x + node.offsetPosition.x;
                trans.position.y = localPos.y + node.offsetPosition.y;
                trans.position.z = localPos.z + node.offsetPosition.z;
                world.AddComponent<TransformComponent>(particleID, std::move(trans));

                if (parentID != 0) {
                    PendingParentComponent pending;
                    pending.parentID = parentID;
                    world.AddComponent<PendingParentComponent>(particleID, pending);
                }

                GPUParticleComponent particle;
                particle.config = node.config;
                if (!node.texturePath.empty())
                    strcpy_s(particle.texturePath,
                        sizeof(particle.texturePath),
                        node.texturePath.c_str());
                if (!node.colorRampPath.empty())
                    strcpy_s(particle.colorRampPath,
                        sizeof(particle.colorRampPath),
                        node.colorRampPath.c_str());
                // 初期化は UpdateSystem がやるので、数値を設定するだけ
                particle.maxParticles = particle.config.max_particle_count;
                particle.needsGpuInit = true;
                world.AddComponent<GPUParticleComponent>(particleID, std::move(particle));

                PooledParticleComponent poolTag;
                poolTag.SetKey(poolKey);
                world.AddComponent<PooledParticleComponent>(particleID, std::move(poolTag));
            }

            float maxLife        = node.config.lifespan.y;
            float targetLifeTime = (parentID == 0) ? (maxLife + 1.0f) : 0.0f;

            if (auto *timer = world.GetComponent<TimerComponent>(particleID)) {
                timer->lifeTime    = targetLifeTime;
                timer->currentTime = 0.0f;
                timer->isExpired   = false;
            }
            else if (targetLifeTime > 0.0f) {
                TimerComponent newTimer;
                newTimer.lifeTime    = targetLifeTime;
                newTimer.currentTime = 0.0f;
                world.AddComponent<TimerComponent>(particleID, std::move(newTimer));
            }
            lastID = particleID;
        }
        return lastID;
    }

    void PrewarmParticles(Core::World &world)
    {
      /*  std::vector<EntityID> allCreatedIDs;
        std::string           firePath = "Assets/Prefabs/Particles/VFX_Env_Fire.json";
        for (int i = 0; i < 20; ++i)
            allCreatedIDs.push_back(SpawnParticle(world, firePath, 0, {0, 0, 0}));
        std::string bulletPath = "Assets/Prefabs/Particles/VFX_Weapon_Bullet.json";
        for (int i = 0; i < 40; ++i)
            allCreatedIDs.push_back(SpawnParticle(world, bulletPath, 0, {0, 0, 0}));
        std::string explPath = "Assets/Prefabs/Particles/VFX_Explosion_Base.json";
        for (int i = 0; i < 40; ++i)
            allCreatedIDs.push_back(SpawnParticle(world, explPath, 0, {0, 0, 0}));

        world.ScrutinyAndApply();

        for (EntityID id : allCreatedIDs) ReturnParticleToPool(world, id);
        world.ScrutinyAndApply();
        OutputDebugStringA("[Prefab] Particles Pre-warmed.\n");*/
    }

    // ===================================================================================
    // 実装: 汎用SpawnPrefab (SceneSerializerを使う)
    // ===================================================================================
    EntityID SpawnPrefab(Core::World &world, const std::string &prefabPath)
    {
        std::string fullPath = prefabPath;
        if (fullPath.find(".json") == std::string::npos) {
            fullPath += ".json";
        }

        // ---------------------------------------------------------
        // 追加: ファイル存在チェックとエラーハンドリング
        // ---------------------------------------------------------
        if (!std::filesystem::exists(fullPath)) {
            // 1. デバッグ出力ウィンドウにエラーを表示
            std::string err = "[Error] Prefab Not Found: " + fullPath + "\n";
            OutputDebugStringA(err.c_str());

            // 2. コンソールにも出す（必要なら）
            // std::cout << err << std::endl;

            // 3. 失敗として無効なIDを返す
            return 0; // または CCL::ECS::InvalidEntityID
        }
        // ---------------------------------------------------------

        // シリアライザを使ってロード
        // JSON内に書かれているコンポーネントが全て復元されます
        std::vector<EntityID> createdEntities = SceneSerializer::Deserialize(&world, fullPath);

        if (createdEntities.empty()) return 0;

        // ルートエンティティ(親なし)を探して返す
        // ※Deserializeが返したリストの0番目がルートとは限らないが、通常はシーンファイルなら0番目
        // 厳密には parentID == 0 を探す
        EntityID rootID = createdEntities[0];
        for (EntityID id : createdEntities) {
            auto *t = world.GetComponent<TransformComponent>(id);
            if (t && t->parentID == 0) {
                rootID = id;
                break;
            }
        }
        return rootID;
    }

    EntityID SpawnPrefab(
        Core::World &world, const std::string &path, const DirectX::XMFLOAT3 &position)
    {
        EntityID entity = SpawnPrefab(world, path);
        if (entity != CCL::ECS::InvalidEntityID) {
            world.PatchComponent<TransformComponent>(
                entity, [position](TransformComponent &trans) { trans.position = position; });
        }
        return entity;
    }

    EntityID SpawnPrefab(Core::World &world,
        const std::string            &path,
        const DirectX::XMFLOAT3      &position,
        const DirectX::XMFLOAT4      &rotation)
    {
        EntityID entity = SpawnPrefab(world, path);
        if (entity != CCL::ECS::InvalidEntityID) {
            world.PatchComponent<TransformComponent>(
                entity, [position, rotation](TransformComponent &trans) {
                    trans.position = position;
                    trans.rotation = rotation;
                });
        }
        return entity;
    }

    EntityID SpawnPrefab(Core::World &world,
        const std::string            &path,
        const DirectX::XMFLOAT3      &position,
        float                         rotationY)
    {
        EntityID entity = SpawnPrefab(world, path);
        if (entity != CCL::ECS::InvalidEntityID) {
            world.PatchComponent<TransformComponent>(
                entity, [position, rotationY](TransformComponent &trans) {
                    trans.position          = position;
                    DirectX::XMVECTOR quatY = DirectX::XMQuaternionRotationRollPitchYaw(
                        0.0f, DirectX::XMConvertToRadians(rotationY), 0.0f);
                    DirectX::XMStoreFloat4(&trans.rotation, quatY);
                });
        }
        return entity;
    }

} // namespace CCL::Prefab