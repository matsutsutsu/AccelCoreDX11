/**
 * @file BehaviorTreeComponentMeta.cpp
 * @brief AI関連コンポーネントのエディタ/シリアライズ用メタデータ定義
 */
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeComponents.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeLoader.h"
#include "Game/Logics/AI/BehaviorTree/Drone/DroneComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/Platform/Dialog.h"
#include <imgui.h>
#include <filesystem>
#include <iterator>

 // =======================================================================
 // エディタ表示用のEnum文字列定義（プルダウンメニュー用）
 // =======================================================================
static const char* DroneFormationNames[] = {
    "Hidden", "OrbitCircle", "SequentialAttack", "DeathRing", "AegisShield",
    "HighOrbit", "LowOrbit", "SpreadLockOn", "AllCharge","CloseGuard", "ChargeTunnel" // ★前回追加した4つを追記
};
static const char* DroneStateNames[] = {
    "Idle", "MoveToTarget", "LockOn", "FireCharge"
};

template <> struct ComponentMeta<BehaviorTreeComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Behavior Tree";
    static constexpr bool hasCustomGui = true; // カスタムGUIを有効化
    static constexpr bool isSerializable = true;

    // --- カスタムGUIでロード機能を統合 ---
    static bool CustomGui(BehaviorTreeComponent& comp, unsigned long long entityID, void* worldPtr) {
        bool changed = false;

        // ★ AnimStateMachineComponent を踏襲した現在のパス表示
        ImGui::TextDisabled("BT File: %s", comp.assetPath.empty() ? "None" : comp.assetPath.c_str());

        // ★ 幅いっぱいの押しやすいロードボタン
        if (ImGui::Button("Load Behavior Tree... (JSON)", ImVec2(-1, 0))) {
            char filename[MAX_PATH] = {};
            // ウィンドウハンドルを渡してダイアログを親ウィンドウの中央に出す
            if (Dialog::OpenFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Select Behavior Tree",
                "Data/BehaviorTree", GetActiveWindow()) == DialogResult::OK) {

                // 絶対パスからプロジェクト相対パスへの変換処理（チーム開発対応）
                namespace fs = std::filesystem;
                fs::path absPath = filename;
                fs::path currentPath = fs::current_path();
                std::error_code ec;
                fs::path relPath = fs::relative(absPath, currentPath, ec);

                // 変換に成功したら相対パスを、失敗したら絶対パスをそのまま保存
                comp.assetPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;

                // メモリバッファが無ければ確保
                if (!comp.sharedAsset) {
                    comp.sharedAsset = std::make_shared<BTAsset>();
                }

                // ★ 即座にロードしてバインドする
                BehaviorTreeLoader::LoadFromJson(comp.assetPath, *comp.sharedAsset);

                // BossAISystem が「ファイルが書き換わった」と勘違いして再ロードするのを防ぐため、
                // 読み込み完了済みのパスとしても登録しておく
                comp.loadedAssetPath = comp.assetPath;

                changed = true;
            }
        }

        ImGui::Separator();

        // 外部のエディタで上書き保存したとき用の手動リロードボタン
        if (ImGui::Button("Reload AI")) {
            if (!comp.sharedAsset) comp.sharedAsset = std::make_shared<BTAsset>();
            BehaviorTreeLoader::LoadFromJson(comp.assetPath, *comp.sharedAsset);
            changed = true;
        }

        ImGui::Separator();


        // nullチェックを入れてからサイズを表示する
        size_t nodeCount = comp.sharedAsset ? comp.sharedAsset->nodes.size() : 0;
        ImGui::LabelText("Nodes", "%zu", nodeCount);
        ImGui::LabelText("Current Node", "%u", comp.runningNodeId);

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            { "AIパス", "assetPath", FieldKind::String, offsetof(BehaviorTreeComponent, assetPath), 0, 0, 0, "File", nullptr, 0, true },
            {"第2形態AIパス", "phase2AssetPath", FieldKind::String, offsetof(BehaviorTreeComponent, phase2AssetPath), 0, 0, 0, "File", nullptr, 0, true },
		    { "第2形態移行HP閾値", "phase2HealthThreshold", FieldKind::Float, offsetof(BehaviorTreeComponent, phase2HealthThreshold), 0.1f, 0.0f, 100.0f, "File", nullptr, 0, true }   
        };
        return fields;
    }
};
REGISTER_COMPONENT(BehaviorTreeComponent, "BehaviorTreeComponent")

// ... (以下 BossCommandComponent と DroneComponent の定義はそのまま) ...

// --- BossCommandComponent（保存しない） ---
template <> struct ComponentMeta<BossCommandComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Boss Command (AI Output)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = false;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
			{ "現在のActionID", "currentActionId", FieldKind::UInt16, offsetof(BossCommandComponent, currentActionId), 1.0f, 0.0f, 0.0f, "Current Command", nullptr, 0, false },

            // ★追加: ドローンに対するフォーメーション要求とターゲットID
            { "フォーメーション要求", "requestFormation", FieldKind::EnumU8, offsetof(BossCommandComponent, requestFormation), 1.0f, 0.0f, 0.0f, "Drone Control", DroneFormationNames, std::size(DroneFormationNames), false },
            { "ターゲット(Player)ID", "targetPlayerId", FieldKind::EntityID, offsetof(BossCommandComponent, targetPlayerId), 1.0f, 0.0f, 0.0f, "Drone Control", nullptr, 0, false }
        };
        return fields;
    }
};
REGISTER_COMPONENT(BossCommandComponent, "BossCommandComponent")


// --- DroneComponent（保存する） ---
template <> struct ComponentMeta<DroneComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Drone (ドローン)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = true;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            // ★修正: EntityID に対応したためコメントアウトを解除
            { "親ボスID", "ownerBossId", FieldKind::EntityID, offsetof(DroneComponent, ownerBossId), 1.0f, 0.0f, 0.0f, "Hierarchy", nullptr, 0, true },
            { "ローカルインデックス", "localIndex", FieldKind::UInt16, offsetof(DroneComponent, localIndex), 1.0f, 0.0f, 0.0f, "Formation", nullptr, 0, true },
            { "総ドローン数", "totalDrones", FieldKind::UInt16, offsetof(DroneComponent, totalDrones), 1.0f, 0.0f, 0.0f, "Formation", nullptr, 0, true },

            // ★追加: 現在のフォーメーションとステートをプルダウン表示
            { "現在の指示", "currentFormation", FieldKind::EnumU8, offsetof(DroneComponent, currentFormation), 1.0f, 0.0f, 0.0f, "State", DroneFormationNames, std::size(DroneFormationNames), false },
            { "現在ステート", "currentState", FieldKind::EnumU8, offsetof(DroneComponent, currentState), 1.0f, 0.0f, 0.0f, "State", DroneStateNames, std::size(DroneStateNames), false },
            { "状態タイマー", "stateTimer", FieldKind::Float, offsetof(DroneComponent, stateTimer), 0.1f, 0.0f, 0.0f, "State", nullptr, 0, false },

            { "目標座標", "targetPosition", FieldKind::Float3, offsetof(DroneComponent, targetPosition), 0.1f, 0.0f, 0.0f, "State", nullptr, 0, false },
            { "移動速度", "moveSpeed", FieldKind::Float, offsetof(DroneComponent, moveSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },
            { "旋回半径", "orbitRadius", FieldKind::Float, offsetof(DroneComponent, orbitRadius), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true }
        };
        return fields;
    }
};
REGISTER_COMPONENT(DroneComponent, "DroneComponent")