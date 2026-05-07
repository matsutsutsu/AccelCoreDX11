#include "AnimationSequencerWindow.h"
#include "Engine/GamePlay/Animation/AnimatorComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "ImSequencer.h"
#include "Engine/Graphics/Resource/Model.h"
#include "Engine/Graphics/Resource/ModelResource.h"
#include "Engine/GamePlay/Animation/Data/AnimSequence.h" 
#include "Engine/GamePlay/Animation/AnimationSystem.h" 

// ボスが持っている武器の名簿を読むためにインクルード
#include "Game/Logics/Combat/CombatRosterComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Transform/PendingParentComponent.h"

#include "Engine/Platform/Dialog.h"
#include "Engine/Platform/Logger.h"
#include "Engine/Graphics/Core/Graphics.h"
#include <filesystem>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <cereal/archives/json.hpp>

// =============================================================================
// データ管理クラス (ImSequencer <-> AnimSequence の仲介)
// =============================================================================
class AnimationSequenceImpl : public ImSequencer::SequenceInterface {
public:
    struct EditorEvent {
        int frameStart;
        int frameEnd;
        int type;
        char stringParam[256];
    };

    std::vector<EditorEvent> cache;
    AnimSequence* targetSequence = nullptr;

    int mFrameMin = 0;
    int mFrameMax = 60 * 5;

    // =========================================================================
    // 台本(ECSデータ) -> UI(ブロック) への変換
    // ※データが区間(Start/End)を持ったため、以前のような結合処理は不要になりました
    // =========================================================================
    void SyncFromData() {
        cache.clear();
        if (!targetSequence) return;

        for (const auto& evt : targetSequence->events) {
            EditorEvent item{};
            item.frameStart = (int)(evt.startTime * 60.0f);
            item.frameEnd = (int)(evt.endTime * 60.0f);

            // 安全な文字列コピー
            strcpy_s(item.stringParam, sizeof(item.stringParam), evt.stringParam.c_str());

            if (evt.eventName == "HitBox") {
                item.type = 0;
                cache.push_back(item);
            }
            else if (evt.eventName == "Play_Sound") {
                item.type = 1;
                cache.push_back(item);
            }
            else if (evt.eventName == "Play_Effect") {
                item.type = 2;
                cache.push_back(item);
            }
            // 過去のデータの互換性維持（もしJSONに古いHitBox_Startが残っていた場合のフェールセーフ）
            else if (evt.eventName == "HitBox_Start") {
                item.type = 0;
                item.frameEnd = item.frameStart + 10; // 適当な長さを付与
                cache.push_back(item);
            }
        }
    }

 
  

    // =========================================================================
    // UI(ブロック) -> 台本(ECSデータ) への変換と自動ソート
    // =========================================================================
    void SyncToData() {
        if (!targetSequence) return;
        targetSequence->events.clear();

        for (const auto& item : cache) {
            float startSec = (float)item.frameStart / 60.0f;
            float endSec = (float)item.frameEnd / 60.0f;

            // push_back に { startTime, endTime, eventName, stringParam } の4つを渡す
            if (item.type == 0) {
                targetSequence->events.push_back({ startSec, endSec, "HitBox", item.stringParam });
            }
            else if (item.type == 1) {
                targetSequence->events.push_back({ startSec, endSec, "Play_Sound", item.stringParam });
            }
            else if (item.type == 2) {
                targetSequence->events.push_back({ startSec, endSec, "Play_Effect", item.stringParam });
            }
        }

        // ソート基準も time から startTime に変更
        std::sort(targetSequence->events.begin(), targetSequence->events.end(),
            [](const AnimNotifyEvent& a, const AnimNotifyEvent& b) {
                return a.startTime < b.startTime;
            });
    }

    void UpdateDuration(float durationSeconds) {
        mFrameMax = (int)(durationSeconds * 60.0f * 1.1f);
        if (mFrameMax < 60) mFrameMax = 60;
    }

    virtual int GetFrameMin() const override { return mFrameMin; }
    virtual int GetFrameMax() const override { return mFrameMax; }
    virtual int GetItemCount() const override { return (int)cache.size(); }
    virtual int GetItemTypeCount() const override { return 3; }

    virtual const char* GetItemTypeName(int typeIndex) const override {
        switch (typeIndex) {
        case 0: return "HitBox (Attack)";
        case 1: return "Sound (SE)";
        case 2: return "Effect (VFX)";
        default: return "Unknown";
        }
    }

    virtual const char* GetItemLabel(int index) const override {
        static char temp[128];
        if (index < 0 || index >= (int)cache.size()) return "";
        const auto& item = cache[index];

        const char* name = (item.stringParam[0] != '\0') ? item.stringParam : "Empty";

        switch (item.type) {
        case 0: sprintf_s(temp, "[%d] Attack: %s", index, name); break;
        case 1: sprintf_s(temp, "[%d] Sound: %s", index, name); break;
        case 2: sprintf_s(temp, "[%d] Effect: %s", index, name); break;
        default: sprintf_s(temp, "[%d] Event", index); break;
        }
        return temp;
    }

    virtual void Get(int index, int** start, int** end, int* type, unsigned int* color) override {
        if (index < 0 || index >= (int)cache.size()) return;
        EditorEvent& item = cache[index];
        if (start) *start = &item.frameStart;
        if (end) *end = &item.frameEnd;
        if (type) *type = item.type;
        if (color) {
            switch (item.type) {
            case 0: *color = 0xFF5050D0; break;
            case 1: *color = 0xFFD08050; break;
            case 2: *color = 0xFF50D050; break;
            default: *color = 0xFFFFFFFF; break;
            }
        }
    }

    virtual void Add(int type) override {
        EditorEvent item;
        item.type = type;
        item.frameStart = 0;
        item.frameEnd = (type == 0) ? 30 : 10;
        memset(item.stringParam, 0, sizeof(item.stringParam));
        cache.push_back(item);
    }

    virtual void Del(int index) override {
        if (index >= 0 && index < (int)cache.size()) cache.erase(cache.begin() + index);
    }
    virtual void Duplicate(int index) override {
        if (index >= 0 && index < (int)cache.size()) cache.push_back(cache[index]);
    }
    virtual size_t GetCustomHeight(int index) override { return 24; }
    virtual void DoubleClick(int index) override {}
    virtual void CustomDraw(int, ImDrawList*, const ImRect&, const ImRect&, const ImRect&, const ImRect&) override {}
    virtual void CustomDrawCompact(int, ImDrawList*, const ImRect&, const ImRect&) override {}
};

// =============================================================================
// Window Class Implementation
// =============================================================================
static AnimSequence g_EditingSequence;
static char g_SavePath[256] = "Assets/Animations/NewSequence.json";
static int s_SelectedAnimIndex = -1; // ターゲットアニメーションのコンボボックス用

AnimationSequencerWindow::AnimationSequencerWindow()
    : EditorWindow("Sequencer"), _expanded(true), _selectedEntry(-1), _firstFrame(0)
{
    _sequencerImpl = std::make_unique<AnimationSequenceImpl>();
    SetVisible(true);
}

AnimationSequencerWindow::~AnimationSequencerWindow() = default;

void AnimationSequencerWindow::DrawContents(EditorContext& context)
{
    if (context.selectedEntity == CCL::ECS::InvalidEntityID || context.selectedEntity == 0) {
        ImGui::TextDisabled("Select an entity to edit animation.");
        return;
    }

    CCL::ECS::EntityID entity = context.selectedEntity;
    auto* animator = context.world->GetComponent<AnimatorComponent>(entity);
    auto* modelComp = context.world->GetComponent<ModelComponent>(entity);

    if (!animator || !modelComp || !modelComp->GetModel()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Entity needs Animator and Model components.");
        return;
    }

    Model* model = modelComp->GetModel();
    ModelResource* resource = model->GetResourceMutable();
    if (!resource) return;

    auto& animations = resource->GetAnimationsMutable();
    if (animations.empty()) return;

    // =========================================================
    // 1. トップツールバー (セーブ・ロード・基本設定)
    // =========================================================
    if (ImGui::Button("Save JSON...")) {
        char filename[MAX_PATH] = {};
        if (Dialog::SaveFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Save Anim Sequence", "json", Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path relPath = fs::relative(filename, fs::current_path(), ec);
            std::string finalPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;

            strcpy_s(g_SavePath, sizeof(g_SavePath), finalPath.c_str());
            _sequencerImpl->SyncToData();

            std::ofstream os(finalPath);
            if (os.is_open()) {
                cereal::JSONOutputArchive archive(os);
                archive(cereal::make_nvp("AnimSequence", g_EditingSequence));
                CCL_LOG_INFO(LogCategory::Editor, "Sequence Saved: %s", finalPath.c_str());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load JSON...")) {
        char filename[MAX_PATH] = {};
        if (Dialog::OpenFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Load Anim Sequence", Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path relPath = fs::relative(filename, fs::current_path(), ec);
            std::string finalPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;

            strcpy_s(g_SavePath, sizeof(g_SavePath), finalPath.c_str());

            std::ifstream is(finalPath);
            if (is.is_open()) {
                cereal::JSONInputArchive archive(is);
                archive(cereal::make_nvp("AnimSequence", g_EditingSequence));

                for (int i = 0; i < (int)animations.size(); ++i) {
                    if (animations[i].name == g_EditingSequence.targetAnimName) {
                        s_SelectedAnimIndex = i; break;
                    }
                }
                _sequencerImpl->targetSequence = &g_EditingSequence;
                _sequencerImpl->UpdateDuration(g_EditingSequence.duration);
                _sequencerImpl->SyncFromData();
                _selectedEntry = -1;
            }
        }
    }
    ImGui::SameLine(); ImGui::TextDisabled("File: %s", g_SavePath[0] == '\0' ? "None" : g_SavePath);

    char seqNameBuf[128] = {};
    strcpy_s(seqNameBuf, sizeof(seqNameBuf), g_EditingSequence.sequenceName.c_str());
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("Sequence Name", seqNameBuf, sizeof(seqNameBuf))) g_EditingSequence.sequenceName = seqNameBuf;

    ImGui::SameLine();
    if (s_SelectedAnimIndex < 0 || s_SelectedAnimIndex >= (int)animations.size()) s_SelectedAnimIndex = 0;
    static int prevSelectedIndex = -2;
    ImGui::SetNextItemWidth(250.0f);
    if (ImGui::BeginCombo("Target Animation", animations[s_SelectedAnimIndex].name.c_str())) {
        for (int i = 0; i < (int)animations.size(); ++i) {
            bool isSelected = (s_SelectedAnimIndex == i);
            if (ImGui::Selectable(animations[i].name.c_str(), isSelected)) s_SelectedAnimIndex = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (prevSelectedIndex != s_SelectedAnimIndex) {
        prevSelectedIndex = s_SelectedAnimIndex;
        g_EditingSequence.targetAnimName = animations[s_SelectedAnimIndex].name;
        g_EditingSequence.duration = animations[s_SelectedAnimIndex].secondsLength;
        _sequencerImpl->targetSequence = &g_EditingSequence;
        _sequencerImpl->UpdateDuration(g_EditingSequence.duration);
        _sequencerImpl->SyncFromData();
    }
    // =========================================================
    // ★改良: Edit Mode の制御（AIの妨害をブロックする）
    // =========================================================
    if (context.isAnimEditMode) {
        // AIによる上書きを防ぎ、エディタの台本を強制セット
        animator->isEditorOverride = true;
        animator->currentSequence = &g_EditingSequence;
    } else {
        // Edit Modeを抜けたらAIに制御を返す
        animator->isEditorOverride = false;
    }

    ImGui::Separator();

    // =========================================================
    // 2. メインレイアウト (左右2ペイン構造)
    // =========================================================
    // Tableを使って画面を 75% : 25% に分割する
    if (ImGui::BeginTable("SequencerLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch, 0.75f);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableNextRow();

        // -----------------------------------------------------
        // 左カラム: タイムラインと追加ボタン
        // -----------------------------------------------------
        ImGui::TableSetColumnIndex(0);

        // エディタ上での再生/停止コントロール
        static bool s_isPlaying = false;
        static bool s_wasEditMode = false; // EditModeに入った瞬間を検知するため

        if (context.isAnimEditMode) {
            // ★改良: Edit Mode に入った瞬間、ゲームの現在時間をシークバーに引き継ぐ（0秒リセットによる固まりを防止）
            if (!s_wasEditMode) {
                context.animEditTime = animator->currentTimer;
                s_wasEditMode = true;
            }

            if (s_isPlaying) {
                float dt = ImGui::GetIO().DeltaTime;
                context.animEditTime += dt * animator->playbackSpeed;
                if (context.animEditTime > g_EditingSequence.duration) {
                    context.animEditTime = animator->isLoop ? fmod(context.animEditTime, g_EditingSequence.duration) : g_EditingSequence.duration;
                }
                animator->currentTimer = context.animEditTime;
            }

            if (ImGui::Button(s_isPlaying ? "|| Pause" : "> Play")) {
                s_isPlaying = !s_isPlaying;
            }
            ImGui::SameLine();
        }
        else {
            s_isPlaying = false;
            s_wasEditMode = false;
        }

        auto AddAndSelectEvent = [&](int type) {
            _sequencerImpl->Add(type);
            auto& newItem = _sequencerImpl->cache.back();
            newItem.frameStart = context.isAnimEditMode ? (int)(context.animEditTime * 60.0f) : 0;
            newItem.frameEnd = newItem.frameStart + ((type == 0) ? 30 : 10);
            _sequencerImpl->SyncToData();
            _selectedEntry = (int)_sequencerImpl->cache.size() - 1;
            };

        if (ImGui::Button("+ Attack (HitBox)")) AddAndSelectEvent(0);
        ImGui::SameLine();
        if (ImGui::Button("+ Sound (SE)")) AddAndSelectEvent(1);
        ImGui::SameLine();
        if (ImGui::Button("+ Effect (VFX)")) AddAndSelectEvent(2);

        int currentFrame = context.isAnimEditMode ? (int)(context.animEditTime * 60.0f) : (int)(animator->currentTimer * 60.0f);
        int prevFrame = currentFrame;

        if (_sequencerImpl->targetSequence) {
            _expanded = true;
            bool changed = ImSequencer::Sequencer(_sequencerImpl.get(), &currentFrame, &_expanded, &_selectedEntry, &_firstFrame,
                ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_CHANGE_FRAME);

            if (changed) _sequencerImpl->SyncToData();

            // ★シークバーをドラッグした瞬間
            if (currentFrame != prevFrame) {
                s_isPlaying = false;
                float newTime = (float)currentFrame / 60.0f;
                context.animEditTime = newTime;
                animator->currentTimer = newTime;
            }

            // 常に最新の animEditTime で骨格と描画を強制更新する
            if (context.isAnimEditMode && context.systemManager) {
                if (auto* animSystem = context.systemManager->GetSystem<AnimationSystem>()) {
                    // ポーズの計算 (ローカル)
                    animSystem->UpdateManual(entity, context.animEditTime);

                    // ★最重要: 計算したポーズを描画用(ワールド)に即座に反映させる！
                    // これがないと描画システムに頂点移動が伝わらず画面がおかしくなる
                    if (auto* transform = context.world->GetComponent<TransformComponent>(entity)) {
                        model->UpdateTransform(transform->worldMatrix);
                    }
                }
            }
        }

        // -----------------------------------------------------
        // 右カラム: 選択中イベントのプロパティ (Inspector)
        // -----------------------------------------------------
        ImGui::TableSetColumnIndex(1);

        ImGui::Text("Duration: %.2fs", g_EditingSequence.duration);
        ImGui::Checkbox("Edit Mode (Preview)", &context.isAnimEditMode);
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Selected Event");
        ImGui::Spacing();

        if (_selectedEntry >= 0 && _selectedEntry < (int)_sequencerImpl->cache.size()) {
            auto& selectedItem = _sequencerImpl->cache[_selectedEntry];
            bool propChanged = false;

            ImGui::TextDisabled("Type: %s", _sequencerImpl->GetItemTypeName(selectedItem.type));
            ImGui::Text("Time: %.2fs - %.2fs", selectedItem.frameStart / 60.0f, selectedItem.frameEnd / 60.0f);
            ImGui::Spacing();

            // HitBox のプロパティ
            if (selectedItem.type == 0) {
                auto* roster = context.world->GetComponent<CombatRosterComponent>(entity);
                if (roster && roster->count > 0) {
                    ImGui::Text("Target Weapon Tag:");
                    const char* currentPreview = selectedItem.stringParam[0] != '\0' ? selectedItem.stringParam : "Select Tag...";
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##HitboxTag", currentPreview)) {
                        for (int i = 0; i < roster->count; ++i) {
                            bool isSelected = (strcmp(selectedItem.stringParam, roster->entries[i].tag) == 0);
                            if (ImGui::Selectable(roster->entries[i].tag, isSelected)) {
                                strcpy_s(selectedItem.stringParam, sizeof(selectedItem.stringParam), roster->entries[i].tag);
                                propChanged = true;
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                else {
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "[Warning] No Roster");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##DamageTag", selectedItem.stringParam, sizeof(selectedItem.stringParam))) propChanged = true;
                }
            }
            // 音・エフェクトのプロパティ
            else if (selectedItem.type == 1 || selectedItem.type == 2) {
                ImGui::Text(selectedItem.type == 1 ? "Audio File Path:" : "Prefab File Path:");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##FilePath", selectedItem.stringParam, sizeof(selectedItem.stringParam))) propChanged = true;

                if (ImGui::Button("Browse...", ImVec2(-1, 0))) {
                    const char* filter = (selectedItem.type == 1) ? "Audio Files\0*.wav;*.mp3;*.ogg\0All Files\0*.*\0" : "Prefab Files\0*.json\0All Files\0*.*\0";
                    char filename[256] = {};
                    if (Dialog::OpenFileName(filename, 256, filter, "Select File", Graphics::Instance().GetWindowHandle()) == DialogResult::OK) {
                        namespace fs = std::filesystem;
                        std::error_code ec;
                        std::string finalPath = fs::relative(filename, fs::current_path(), ec).generic_string();
                        strcpy_s(selectedItem.stringParam, sizeof(selectedItem.stringParam), finalPath.c_str());
                        propChanged = true;
                    }
                }
            }

            if (propChanged) _sequencerImpl->SyncToData();
        }
        else {
            ImGui::TextDisabled("No event block selected.\nClick a block on the timeline.");
        }

        ImGui::EndTable();
    }
}
