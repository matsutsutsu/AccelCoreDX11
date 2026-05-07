#include "ECS/Core/CCL_World.h"
#include "Game/Logic/Combat/CombatComponents.h"
#include "Game/Logic/Combat/CombatRosterComponent.h"
#include "Engine/GamePlay/Transform/BoneAttachmentComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

#include "Game/Logic/Combat/HealthComponent.h" // ※環境に合わせて適宜パスを調整してください
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Transform/PendingParentComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/Assets/Model.h"
#include "Engine/Serialization/SerializationContext.h"

// ===================================================================
// 1. HitboxComponent (攻撃判定)
// ===================================================================
template <> struct ComponentMeta<HitboxComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Hitbox (攻撃判定)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = true;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(HitboxComponent, damageAmount, "damageAmount", "基礎ダメージ量", 1.0f, 0.0f, 9999.0f, "Combat"),
            META_FIELD_BOOL(HitboxComponent, isActive, "isActive", "判定アクティブ(Debug)", "Combat"),
            // ※ hitTargets や hitCount はランタイムの内部状態なのでインスペクタには露出させない

            // インスペクタから直接ヒットストップを設定可能にする
            META_FIELD_FLOAT(HitboxComponent, hitStopDuration, "hitStopDuration", "ヒットストップ(秒)", 0.0f, 0.0f, 1.0f, "Hit Stop"),
            META_FIELD_FLOAT(HitboxComponent, hitStopFreezeScale, "hitStopFreezeScale", "停止スケール", 0.0f, 0.0f, 1.0f, "Hit Stop")

        };
        return fields;
    }
};
REGISTER_COMPONENT(HitboxComponent, "HitboxComponent");

// ===================================================================
// 2. HurtboxComponent (被弾判定)
// ===================================================================
template <> struct ComponentMeta<HurtboxComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Hurtbox (被弾判定)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = true;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(HurtboxComponent, damageMultiplier, "ダメージ倍率 (弱点等)", "Multiplier", 0.1f, 0.0f, 10.0f, "Combat")
        };
        return fields;
    }
};
REGISTER_COMPONENT(HurtboxComponent, "HurtboxComponent");

// ===================================================================
// 3. BoneAttachmentComponent (骨格追従)
// ===================================================================
template <> struct ComponentMeta<BoneAttachmentComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Bone Attachment (ボーン追従)";
    static constexpr bool hasCustomGui = true;
    static constexpr bool isSerializable = true;

    static bool CustomGui(BoneAttachmentComponent& comp, unsigned long long entityID, void* worldPtr) {
        bool changed = false;
        auto* world = static_cast<CCL::ECS::Core::World*>(worldPtr);
        if (!world) return false;

        CCL::ECS::EntityID parentID = 0;
        if (auto* trans = world->GetComponent<TransformComponent>(entityID)) {
            parentID = trans->parentID;
        }
        if (parentID == 0) {
            if (auto* pending = world->GetComponent<PendingParentComponent>(entityID)) {
                parentID = pending->parentID;
            }
        }

        ImGui::Text("Target Bone Name:");
        ImGui::SetNextItemWidth(-1);

        if (parentID != 0) {
            if (auto* modelComp = world->GetComponent<ModelComponent>(parentID)) {
                if (Model* model = modelComp->GetModel()) {
                    const auto& nodes = model->GetNodes();
                    if (!nodes.empty()) {

                        // std::string の empty() で判定
                        const char* currentPreview = !comp.boneName.empty() ? comp.boneName.c_str() : "Select Bone...";

                        if (ImGui::BeginCombo("##BoneCombo", currentPreview)) {

                            // "None" の選択
                            if (ImGui::Selectable("None", comp.boneName.empty())) {
                                comp.boneName = "";
                                comp.cachedBoneIndex = -1;
                                changed = true;
                            }

                            for (int i = 0; i < nodes.size(); ++i) {
                                // std::string の == 演算子で比較
                                bool isSelected = (comp.boneName == nodes[i].m_Name);
                                if (ImGui::Selectable(nodes[i].m_Name.c_str(), isSelected)) {
                                    // std::string の代入演算子を使用
                                    comp.boneName = nodes[i].m_Name;
                                    comp.cachedBoneIndex = -1;
                                    changed = true;
                                }
                                if (isSelected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        return changed;
                    }
                }
            }
        }

        ImGui::TextColored(ImVec4(1, 1, 0, 1), "[Warning] Parent Model not found.");

        // ★ ImGui::InputText と std::string の連携
        // バッファにコピーして編集し、終わったら書き戻す
        char buffer[256];
        strncpy_s(buffer, sizeof(buffer), comp.boneName.c_str(), _TRUNCATE);
        if (ImGui::InputText("##BoneNameInput", buffer, sizeof(buffer))) {
            comp.boneName = buffer;
            comp.cachedBoneIndex = -1;
            changed = true;
        }

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        // ★ 究極の修正1: JSONシリアライザに保存する変数を教える
        static const std::vector<FieldDescriptor> fields = {
            // std::string boneName を "boneName" というキーで JSON に保存する
            META_FIELD_STRING(BoneAttachmentComponent, boneName, "boneName", "Bone Name", "BoneAttachment")
        };
        // ※ cachedBoneIndex は実行時に計算するため保存不要
        return fields;
    }
};
REGISTER_COMPONENT(BoneAttachmentComponent, "BoneAttachmentComponent");

// ComponentMetaJson.h の Serialize / Deserialize が
// このコンポーネントだけカスタム実装を呼ぶようになる
template <>
struct ComponentMetaJson::HasCustomSerialize<CombatRosterComponent> : std::true_type {};


// ===================================================================
// 4. CombatRosterComponent (武器・部位名簿)
// ===================================================================
template <> struct ComponentMeta<CombatRosterComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Combat Roster (部位名簿)";
    static constexpr bool hasCustomGui = true;
    static constexpr bool isSerializable = true;
    static constexpr bool        hasCustomSerialize = true;


    static bool CustomGui(CombatRosterComponent& comp, unsigned long long entityID, void* worldPtr) {
        bool changed = false;
        auto* world = static_cast<CCL::ECS::Core::World*>(worldPtr);
        if (!world) return false;

        // ------------------------------------------------------------------
        // [準備] 親エンティティ（ボス）の「子エンティティ」のリストを収集する
        // ------------------------------------------------------------------
        std::vector<CCL::ECS::EntityID> childEntities;
        if (auto* trans = world->GetComponent<TransformComponent>(entityID)) {
            CCL::ECS::EntityID child = trans->firstChildID;
            while (child != 0) {
                childEntities.push_back(child);
                if (auto* childTrans = world->GetComponent<TransformComponent>(child)) {
                    child = childTrans->nextSiblingID;
                }
                else break;
            }
        }

        ImGui::Text("登録済みの部位/武器タグ:");
        ImGui::Separator();

        for (int i = 0; i < comp.count; ++i) {
            ImGui::PushID(i);

            // ヘッダーと削除ボタン
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Slot %d", i);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 30.0f);
            if (ImGui::Button("X")) {
                // 配列を詰めて削除する処理
                for (int j = i; j < comp.count - 1; ++j) {
                    comp.entries[j] = comp.entries[j + 1];
                }
                comp.count--;
                changed = true;
                ImGui::PopID();
                break; // 削除したフレームはループを抜けて次回再描画
            }

            // --------------------------------------------------------------
            // ① Tagの入力 (プリセットからの選択 ＋ 自由入力)
            // --------------------------------------------------------------
            ImGui::Text(" Tag  "); ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);

            const char* currentPreview = comp.entries[i].tag[0] != '\0' ? comp.entries[i].tag : "Select...";
            if (ImGui::BeginCombo("##TagCombo", currentPreview)) {
                // AAAゲームでよく使われる汎用タグのプリセット
                const char* presets[] = { "RightHand", "LeftHand", "RightFoot", "LeftFoot", "Head", "Tail", "Weapon_A", "Weapon_B" };
                for (const char* p : presets) {
                    if (ImGui::Selectable(p, strcmp(comp.entries[i].tag, p) == 0)) {
                        strcpy_s(comp.entries[i].tag, sizeof(comp.entries[i].tag), p);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1); // 残りの幅を手打ち用に
            if (ImGui::InputText("##TagInput", comp.entries[i].tag, sizeof(comp.entries[i].tag))) {
                changed = true; // プルダウンにない特殊な名前の手打ちも許可する
            }

            // --------------------------------------------------------------
            // ② 紐付ける Entity の選択 (子エンティティリストから選ぶ)
            // --------------------------------------------------------------
            ImGui::Text(" Bind "); ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            char entityPreview[64];
            if (comp.entries[i].id == 0) sprintf_s(entityPreview, "None (Not Bound)");
            else sprintf_s(entityPreview, "Entity [%llu]", comp.entries[i].id);

            if (ImGui::BeginCombo("##EntityCombo", entityPreview)) {
                // バインド解除用
                if (ImGui::Selectable("None", comp.entries[i].id == 0)) {
                    comp.entries[i].id = 0;
                    changed = true;
                }
                // 収集した子エンティティをリスト表示
                for (auto childID : childEntities) {
                    char label[64];
                    sprintf_s(label, "Child Entity [%llu]", childID);
                    if (ImGui::Selectable(label, comp.entries[i].id == childID)) {
                        comp.entries[i].id = childID;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (comp.count < 4) {
            if (ImGui::Button("+ 新しい部位を追加", ImVec2(-1, 0))) {
                comp.entries[comp.count].id = 0;
                comp.entries[comp.count].tag[0] = '\0';
                comp.count++;
                changed = true;
            }
        }
        else {
            ImGui::TextDisabled("最大登録数(4)に達しています");
        }

        return changed;
    }

    // ★ カスタム保存
    static void serialize(nlohmann::json& j, const void* ptr)
    {
        const auto& comp = *static_cast<const CombatRosterComponent*>(ptr);
        j["count"] = comp.count;

        j["entries"] = nlohmann::json::array();
        for (int i = 0; i < comp.count; ++i)
        {
            nlohmann::json entry;
            entry["tag"] = comp.entries[i].tag;
            entry["id"] = static_cast<uint64_t>(comp.entries[i].id);
            j["entries"].push_back(entry);
        }
    }

    // ★ カスタム読み込み  ← ここが今まで存在しなかった部分
    static void deserializeToWorld(
        CCL::ECS::Core::World* world,
        CCL::ECS::EntityID           entity,
        const nlohmann::json& j)
    {
        CombatRosterComponent comp{};

        if (j.contains("count"))
        {
            comp.count = j["count"].get<int>();
            // 安全のため上限を超えないようにクランプ
            if (comp.count > 4) comp.count = 4;
        }

        if (j.contains("entries") && j["entries"].is_array())
        {
            int idx = 0;
            for (const auto& entry : j["entries"])
            {
                if (idx >= comp.count) break;

                // tag (char[32]) の復元
                if (entry.contains("tag"))
                {
                    std::string tag = entry["tag"].get<std::string>();
                    strncpy_s(
                        comp.entries[idx].tag,
                        sizeof(comp.entries[idx].tag),
                        tag.c_str(),
                        _TRUNCATE);
                }

                // id の復元
                // ※ EntityID のリマップは SceneSerializer の Pass3 が行うので、
                //   ここでは JSON の生の値をそのまま入れておく
                if (entry.contains("id"))
                {
                    comp.entries[idx].id = static_cast<CCL::ECS::EntityID>(
                        entry["id"].get<uint64_t>());
                }

                ++idx;
            }
        }

        world->AddComponent<CombatRosterComponent>(entity, std::move(comp));
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        // カスタムシリアライズのため空でよい
        static const std::vector<FieldDescriptor> empty;
        return empty;
    }

};
REGISTER_COMPONENT(CombatRosterComponent, "CombatRosterComponent");


// ===================================================================
// 5. DamageEventComponent (ダメージイベント)
// ===================================================================
// 【設計思想】システム間通信用の「揮発性手紙」なので、絶対にセーブしてはならない (isSerializable = false)
template <> struct ComponentMeta<DamageEventComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Damage Event (一時データ)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = false; // ★保存しない

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {};
        return fields;
    }
};
REGISTER_COMPONENT(DamageEventComponent, "DamageEventComponent");

template <> struct ComponentMeta<JustEvadeEventComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "JustEvade Event (一時データ)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = false; // ★保存しない

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {};
        return fields;
    }
};
REGISTER_COMPONENT(JustEvadeEventComponent, "JustEvadeEventComponent");

// ===================================================================
// 6. HealthComponent (体力・陣営データ)
// ===================================================================

// JSONシリアライザにカスタム関数を使用することを通知
template <>
struct ComponentMetaJson::HasCustomSerialize<HealthComponent> : std::true_type {};

template <> struct ComponentMeta<HealthComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Health (体力・陣営)";
    static constexpr bool hasCustomGui = true;
    static constexpr bool isSerializable = true;
    static constexpr bool hasCustomSerialize = true;

    static bool CustomGui(HealthComponent& comp, unsigned long long entityID, void* worldPtr) {
        bool changed = false;

        // -----------------------------------------------------------
        // 1. 陣営 (TeamID) のプルダウン選択
        // -----------------------------------------------------------
        const char* teamNames[] = { "Player (プレイヤー)", "Enemy (敵)", "Neutral (中立)" };
        int currentTeam = static_cast<int>(comp.team);

        ImGui::Text("Team / Faction:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##TeamCombo", &currentTeam, teamNames, IM_ARRAYSIZE(teamNames))) {
            comp.team = static_cast<TeamID>(currentTeam);
            changed = true;
        }

        ImGui::Separator();

        // -----------------------------------------------------------
        // 2. 体力バーとエディタ
        // -----------------------------------------------------------
        ImGui::Text("Health Status:");

        // プログレスバーによる視覚的な体力表示
        float healthRatio = comp.maxHealth > 0.0f ? (comp.currentHealth / comp.maxHealth) : 0.0f;
        char overlay[32];
        sprintf_s(overlay, "%.1f / %.1f", comp.currentHealth, comp.maxHealth);
        ImGui::ProgressBar(healthRatio, ImVec2(-1.f, 0.f), overlay);

        // 最大値と現在値の安全なスライダー入力
        if (ImGui::DragFloat("Max Health", &comp.maxHealth, 1.0f, 1.0f, 99999.0f)) {
            // 最大HPを下げた際、現在HPが飛び出さないように自動クランプする
            if (comp.currentHealth > comp.maxHealth) {
                comp.currentHealth = comp.maxHealth;
            }
            changed = true;
        }
        if (ImGui::DragFloat("Current Health", &comp.currentHealth, 1.0f, 0.0f, comp.maxHealth)) {
            changed = true;
        }

        ImGui::Separator();

        // -----------------------------------------------------------
        // 3. 無敵時間の設定と、ランタイム状態のモニタリング
        // -----------------------------------------------------------
        ImGui::Text("Invincibility (無敵設定):");
        if (ImGui::DragFloat("Duration (sec)", &comp.invincibilityDuration, 0.05f, 0.0f, 10.0f)) {
            changed = true;
        }

        // デバッグ表示: ゲーム実行中に無敵タイマーが動いているかを視覚化
        if (comp.invincibilityTimer > 0.0f) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), ">> Invincible! (%.2f sec left) <<", comp.invincibilityTimer);
        }
        else {
            ImGui::TextDisabled("Not Invincible");
        }

        return changed;
    }

    // ★ カスタム保存 (タイマーなどの不要なランタイム状態は保存しない)
    static void serialize(nlohmann::json& j, const void* ptr) {
        const auto& comp = *static_cast<const HealthComponent*>(ptr);

        j["maxHealth"] = comp.maxHealth;
        j["currentHealth"] = comp.currentHealth;
        j["invincibilityDuration"] = comp.invincibilityDuration;
        j["team"] = static_cast<int>(comp.team); // Enum は int として保存

        // ※ invincibilityTimer は絶対に保存しない（ロード直後に無敵になるバグを防ぐため）
    }

    // ★ カスタム読み込み
    static void deserializeToWorld(CCL::ECS::Core::World* world, CCL::ECS::EntityID entity, const nlohmann::json& j) {
        HealthComponent comp{};

        if (j.contains("maxHealth"))             comp.maxHealth = j["maxHealth"].get<float>();
        if (j.contains("currentHealth"))         comp.currentHealth = j["currentHealth"].get<float>();
        if (j.contains("invincibilityDuration")) comp.invincibilityDuration = j["invincibilityDuration"].get<float>();
        if (j.contains("team"))                  comp.team = static_cast<TeamID>(j["team"].get<int>());

        world->AddComponent<HealthComponent>(entity, std::move(comp));
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> empty;
        return empty;
    }
};
REGISTER_COMPONENT(HealthComponent, "HealthComponent");