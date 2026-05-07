#include "AnimationSequencerWindow.h"
#include "Engine/GamePlay/Animation/AnimatorComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "ImSequencer.h"
#include "Engine/Assets/Model.h"
#include "Engine/Assets/ModelResource.h"
#include "Engine/GamePlay/Animation/Data/AnimSequence.h" 
#include "Engine/GamePlay/Animation/AnimationSystem.h" 

// ボスが持っている武器の名簿を読むためにインクルード
#include "Game/Logic/Combat/CombatRosterComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Transform/PendingParentComponent.h"

#include "Engine/Platform/Dialog.h"
#include "Engine/Platform/Logger.h"

#include <filesystem>

#include <ImCurveEdit.h>
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

        // ヒットストップ用のパラメータをキャッシュできるようにする
        float floatParam1 = 0.0f;
        float floatParam2 = 0.0f;
    };

    std::vector<EditorEvent> cache;
    AnimSequence* targetSequence = nullptr;

    int pendingBBStart = -1;
    std::string pendingBBEventName = "";

    int mFrameMin = 0;
    int mFrameMax = 60 * 5;

    // =========================================================================
    // 台本(ECSデータ) -> UI(ブロック) への変換
    // =========================================================================
    void SyncFromData() {
        cache.clear();
        if (!targetSequence) return;

        int pendingBBStart = -1;
        std::string pendingBBEventName = "";

        for (const auto& evt : targetSequence->events) {
            EditorEvent item{};
            item.frameStart = (int)(evt.startTime * 60.0f);
            item.frameEnd = (int)(evt.endTime * 60.0f);

            strcpy_s(item.stringParam, sizeof(item.stringParam), evt.stringParam.c_str());
            
            // ヒットストップ用のパラメータをキャッシュ
            item.floatParam1 = evt.floatParam1;
            item.floatParam2 = evt.floatParam2;

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
            else if (evt.eventName.find("BB_") == 0 && evt.eventName.find("_True") != std::string::npos) {
                item.type = 3;
                pendingBBStart = (int)(evt.startTime * 60.0f);
                pendingBBEventName = evt.eventName.substr(3, evt.eventName.find("_True") - 3);
            }
            else if (evt.eventName.find("BB_") == 0 && evt.eventName.find("_False") != std::string::npos) {
                if (pendingBBStart != -1) {
                    item.type = 3;
                    EditorEvent item{ pendingBBStart, (int)(evt.startTime * 60.0f), 3 };
                    strcpy_s(item.stringParam, sizeof(item.stringParam), pendingBBEventName.c_str());
                    cache.push_back(item);
                    pendingBBStart = -1;
                }
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

            // 1. まず共通のイベントデータ(ev)を作成し、パラメータを詰め込む
            AnimNotifyEvent ev;
            ev.startTime = startSec;
            ev.endTime = endSec;
            ev.floatParam1 = item.floatParam1; // ヒットストップ時間など
            ev.floatParam2 = item.floatParam2; // スロー倍率など

            // 2. 種類(type)に応じて名前と文字列パラメータを設定し、台本に保存
            if (item.type == 0) {
                ev.eventName = "HitBox";
                ev.stringParam = item.stringParam;
                targetSequence->events.push_back(ev);
            }
            else if (item.type == 1) {
                ev.eventName = "Play_Sound";
                ev.stringParam = item.stringParam;
                targetSequence->events.push_back(ev);
            }
            else if (item.type == 2) {
                ev.eventName = "Play_Effect";
                ev.stringParam = item.stringParam;
                targetSequence->events.push_back(ev);
            }
            else if (item.type == 3) {
                // Blackboardの場合は、開始時にTrue、終了時にFalseの2つのイベントを生成する
                std::string baseName = item.stringParam;

                ev.eventName = "BB_" + baseName + "_True";
                ev.stringParam = "";
                targetSequence->events.push_back(ev);

                AnimNotifyEvent evFalse = ev; // パラメータをコピー
                evFalse.startTime = endSec;   // Falseイベントはブロックの終了時間に発火する
                evFalse.eventName = "BB_" + baseName + "_False";
                targetSequence->events.push_back(evFalse);
            }
        }

        // 時間順にソートしてバグを防ぐ
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
    virtual int GetItemTypeCount() const override { return 4; }

    virtual const char* GetItemTypeName(int typeIndex) const override {
        switch (typeIndex) {
        case 0: return "HitBox (Attack)";
        case 1: return "Sound (SE)";
        case 2: return "Effect (VFX)";
        case 3: return "Blackboard (Flag)";
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
        case 3: sprintf_s(temp, "[%d] Blackboard: %s", index, name); break;
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
// SequencerCurveDelegate
// =============================================================================
struct SequencerCurveDelegate : public ImCurveEdit::Delegate {
    AnimSequence* targetSeq = nullptr;
    ImVec2 points[2][256];
    bool visible[2] = { true, true };

    ImVec2 m_min = ImVec2(0.0f, -1.0f);
    ImVec2 m_max = ImVec2(1.0f, 3.0f);

    // アニメーションの実際の長さ（Duration ラインの表示用）
    float animDuration = 1.0f;

    void SetSequence(AnimSequence* seq) {
        targetSeq = seq;
        if (targetSeq) {
            animDuration = targetSeq->duration;
            // X最大値はdurationだけ固定せず、ユーザーが広げられるようにする
            // 初回だけ自動フィット
            if (m_max.x <= 0.0f || m_max.x == 1.0f) {
                m_max.x = targetSeq->duration;
            }
        }
        UpdatePoints();
    }

    void FitViewToDuration() {
        if (!targetSeq) return;
        m_min.x = 0.0f;
        m_max.x = targetSeq->duration;
    }

    void UpdatePoints() {
        if (!targetSeq) return;
        for (size_t i = 0; i < targetSeq->speedCurve.keys.size() && i < 256; ++i) {
            points[0][i] = ImVec2(targetSeq->speedCurve.keys[i].time, targetSeq->speedCurve.keys[i].value);
        }
        for (size_t i = 0; i < targetSeq->rootMotionCurve.keys.size() && i < 256; ++i) {
            points[1][i] = ImVec2(targetSeq->rootMotionCurve.keys[i].time, targetSeq->rootMotionCurve.keys[i].value);
        }
    }

    size_t GetCurveCount() override { return 2; }
    size_t GetPointCount(size_t curveIndex) override {
        if (!targetSeq) return 0;
        return (curveIndex == 0) ? targetSeq->speedCurve.keys.size() : targetSeq->rootMotionCurve.keys.size();
    }

    ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const override { return ImCurveEdit::CurveSmooth; }
    bool IsVisible(size_t curveIndex) override { return visible[curveIndex]; }
    uint32_t GetCurveColor(size_t curveIndex) override { return curveIndex == 0 ? 0xFF00FF00 : 0xFF00A5FF; }
    ImVec2* GetPoints(size_t curveIndex) override { return points[curveIndex]; }
    virtual ImVec2& GetMin() override { return m_min; }
    virtual ImVec2& GetMax() override { return m_max; }
    virtual unsigned int GetBackgroundColor() override {
        return 0x00000000; // 変更点: 背景を「完全な透明」にし、下敷きのグリッドを見せる
    }

    int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override {
        if (!targetSeq) return pointIndex;

        // 変更点: 上限のクランプを外し、右側に自由にキーを打てるようにする
        value.x = (std::max)(0.0f, value.x);

        AnimationCurve& curve = (curveIndex == 0) ? targetSeq->speedCurve : targetSeq->rootMotionCurve;
        curve.keys[pointIndex].time = value.x;
        curve.keys[pointIndex].value = value.y;
        UpdatePoints();
        return pointIndex;
    }

    void AddPoint(size_t curveIndex, ImVec2 value) override {
        if (!targetSeq) return;
        AnimationCurve& curve = (curveIndex == 0) ? targetSeq->speedCurve : targetSeq->rootMotionCurve;
        curve.keys.push_back({ value.x, value.y });
        std::sort(curve.keys.begin(), curve.keys.end(), [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
        UpdatePoints();
    }
};

// =============================================================================
// ★新機能: カーブエディタ グリッド＋ルーラー オーバーレイ描画
// ImCurveEdit::Edit の「後」に呼び出し、グリッド線と目盛りラベルを重ね描きする
// =============================================================================
static void DrawCurveEditorOverlay(
    ImDrawList* dl,
    ImVec2       canvasPos,      // Edit()描画領域の左上スクリーン座標
    ImVec2       canvasSize,     // Edit()描画領域のサイズ
    ImVec2       viewMin,        // データ空間の表示最小値 (x=時間, y=値)
    ImVec2       viewMax,        // データ空間の表示最大値
    float        animDuration    // アニメーション終端位置 (縦線を引く)
) {
    if (canvasSize.x <= 0 || canvasSize.y <= 0) return;
    if (viewMax.x <= viewMin.x || viewMax.y <= viewMin.y) return;

    const float left = canvasPos.x;
    const float top = canvasPos.y;
    const float right = left + canvasSize.x;
    const float bottom = top + canvasSize.y;

    // ImCurveEdit 内部のパディング（実装に合わせて微調整してください）
    const float PAD = 6.0f;

    dl->PushClipRect(canvasPos, ImVec2(right, bottom), true);

    // --- 色の定義 ---
    const ImU32 COL_GRID_MINOR = IM_COL32(70, 70, 70, 110);  // 細グリッド
    const ImU32 COL_GRID_MAJOR = IM_COL32(100, 100, 100, 160);  // 太グリッド
    const ImU32 COL_AXIS_ZERO = IM_COL32(180, 180, 180, 200);  // ゼロライン
    const ImU32 COL_DURATION = IM_COL32(255, 180, 60, 200);  // Duration ライン
    const ImU32 COL_LABEL = IM_COL32(210, 210, 210, 230);  // ラベル文字
    const ImU32 COL_LABEL_SHD = IM_COL32(0, 0, 0, 200);  // 文字シャドウ
    const ImU32 COL_RULER_BG = IM_COL32(25, 25, 25, 220);  // ルーラー背景

    // --- データ座標 → スクリーン座標 変換 ---
    auto toScreenX = [&](float t) -> float {
        return left + (t - viewMin.x) / (viewMax.x - viewMin.x) * canvasSize.x;
        };
    auto toScreenY = [&](float v) -> float {
        return bottom - (v - viewMin.y) / (viewMax.y - viewMin.y) * canvasSize.y;
        };

    // =========================================================
    // 1. X軸 (時間) グリッド ─ 間隔を表示範囲に合わせて自動選択
    // =========================================================
    const float timeRange = viewMax.x - viewMin.x;
    float timeStep = 1.0f;
    if (timeRange <= 0.3f)  timeStep = 0.02f;
    else if (timeRange <= 1.0f)  timeStep = 0.05f;
    else if (timeRange <= 2.0f)  timeStep = 0.1f;
    else if (timeRange <= 5.0f)  timeStep = 0.25f;
    else if (timeRange <= 15.0f) timeStep = 0.5f;
    else                          timeStep = 1.0f;

    const float timeStart = std::floor(viewMin.x / timeStep) * timeStep;
    for (float t = timeStart; t <= viewMax.x + timeStep * 0.01f; t += timeStep) {
        const float sx = toScreenX(t);
        if (sx < left - 1.0f || sx > right + 1.0f) continue;

        // 整数秒は少し太く明るく
        const bool isMajor = (fabsf(fmodf(t + timeStep * 0.001f, 1.0f)) < timeStep * 0.5f);
        dl->AddLine(ImVec2(sx, top), ImVec2(sx, bottom),
            isMajor ? COL_GRID_MAJOR : COL_GRID_MINOR,
            isMajor ? 1.2f : 0.8f);
    }

    // =========================================================
    // 2. Y軸 (値) グリッド
    // =========================================================
    const float valueRange = viewMax.y - viewMin.y;
    float valueStep = 0.5f;
    if (valueRange <= 0.5f)  valueStep = 0.05f;
    else if (valueRange <= 2.0f)  valueStep = 0.25f;
    else if (valueRange <= 5.0f)  valueStep = 0.5f;
    else if (valueRange <= 10.0f) valueStep = 1.0f;
    else                           valueStep = 2.0f;

    const float valueStart = std::floor(viewMin.y / valueStep) * valueStep;
    for (float v = valueStart; v <= viewMax.y + valueStep * 0.01f; v += valueStep) {
        const float sy = toScreenY(v);
        if (sy < top - 1.0f || sy > bottom + 1.0f) continue;

        // Y=0 のゼロラインは特別扱い
        if (fabsf(v) < valueStep * 0.1f) {
            dl->AddLine(ImVec2(left, sy), ImVec2(right, sy), COL_AXIS_ZERO, 1.5f);
        }
        else {
            dl->AddLine(ImVec2(left, sy), ImVec2(right, sy), COL_GRID_MINOR, 0.8f);
        }
    }

    // =========================================================
    // 3. Duration 終端ライン (オレンジ縦線)
    // =========================================================
    {
        const float sx = toScreenX(animDuration);
        if (sx >= left && sx <= right) {
            dl->AddLine(ImVec2(sx, top), ImVec2(sx, bottom), COL_DURATION, 1.5f);
            // "END" ラベル
            const ImVec2 tp(sx + 3.0f, top + 2.0f);
            dl->AddText(ImVec2(tp.x + 1, tp.y + 1), COL_LABEL_SHD, "END");
            dl->AddText(tp, COL_DURATION, "END");
        }
    }

    // =========================================================
    // 4. 下部ルーラー: 時間ラベル
    // =========================================================
    const float RULER_H = 16.0f;
    dl->AddRectFilled(ImVec2(left, bottom - RULER_H), ImVec2(right, bottom), COL_RULER_BG);

    for (float t = timeStart; t <= viewMax.x + timeStep * 0.01f; t += timeStep) {
        const float sx = toScreenX(t);
        if (sx < left + 2.0f || sx > right - 2.0f) continue;

        // ティック
        dl->AddLine(ImVec2(sx, bottom - RULER_H), ImVec2(sx, bottom - RULER_H + 4.0f), COL_GRID_MAJOR, 1.0f);

        // 時間テキスト
        char buf[24];
        if (timeStep < 0.1f)      snprintf(buf, sizeof(buf), "%.3fs", t);
        else if (timeStep < 1.0f) snprintf(buf, sizeof(buf), "%.2fs", t);
        else                       snprintf(buf, sizeof(buf), "%.1fs", t);

        const ImVec2 tp(sx + 2.0f, bottom - RULER_H + 3.0f);
        dl->AddText(ImVec2(tp.x + 1, tp.y + 1), COL_LABEL_SHD, buf);
        dl->AddText(tp, COL_LABEL, buf);
    }

    // =========================================================
    // 5. 左部ルーラー: 値ラベル
    // =========================================================
    const float VALUE_RULER_W = 38.0f;
    dl->AddRectFilled(ImVec2(left, top), ImVec2(left + VALUE_RULER_W, bottom - RULER_H), COL_RULER_BG);

    for (float v = valueStart; v <= viewMax.y + valueStep * 0.01f; v += valueStep) {
        const float sy = toScreenY(v);
        if (sy < top + 4.0f || sy > bottom - RULER_H - 4.0f) continue;

        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f", v);

        const ImVec2 tp(left + 2.0f, sy - 7.0f);
        dl->AddText(ImVec2(tp.x + 1, tp.y + 1), COL_LABEL_SHD, buf);
        dl->AddText(tp, COL_LABEL, buf);

        // ルーラーから本体への短いティック線
        dl->AddLine(ImVec2(left + VALUE_RULER_W - 3.0f, sy),
            ImVec2(left + VALUE_RULER_W + 3.0f, sy),
            COL_GRID_MAJOR, 1.0f);
    }

    // =========================================================
    // 6. 外枠
    // =========================================================
    dl->AddRect(canvasPos, ImVec2(right, bottom), IM_COL32(90, 90, 90, 200));

    dl->PopClipRect();
}

// =============================================================================
// Window Class Implementation
// =============================================================================
static AnimSequence g_EditingSequence;
static char g_SavePath[256] = "Assets/Animations/NewSequence.json";
static int s_SelectedAnimIndex = -1;

AnimationSequencerWindow::AnimationSequencerWindow()
    : EditorWindow("Sequencer"), _expanded(true), _selectedEntry(-1), _firstFrame(0)
{
    _sequencerImpl = std::make_unique<AnimationSequenceImpl>();
    _curveDelegate = std::make_unique<SequencerCurveDelegate>();

    //  初回から空のタイムラインを表示するために紐付けを行う
    _sequencerImpl->targetSequence = &g_EditingSequence;
    _sequencerImpl->UpdateDuration(g_EditingSequence.duration);
    _sequencerImpl->SyncFromData();

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
        if (Dialog::SaveFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Save Anim Sequence", "json",
            "Data/Animations/Sequence", GetActiveWindow()) == DialogResult::OK) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path relPath = fs::relative(filename, fs::current_path(), ec);
            std::string finalPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;

            strcpy_s(g_SavePath, sizeof(g_SavePath), finalPath.c_str());
            _sequencerImpl->SyncToData();

            std::ofstream os(finalPath);
            if (os.is_open()) {
                // ★修正: 厳格な Cereal を廃止し、柔軟な nlohmann::json でセーブする
                nlohmann::json j;
                // Cereal が作っていた古いフォーマットと互換性を保つため、キーで包む
                j["AnimSequence"] = g_EditingSequence;
                os << j.dump(4);

                CCL_LOG_INFO(LogCategory::Editor, "Sequence Saved: %s", finalPath.c_str());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load JSON...")) {
        char filename[MAX_PATH] = {};
        if (Dialog::OpenFileName(filename, MAX_PATH, "JSON Files\0*.json\0", "Load Anim Sequence",
            "Data/Animations/Sequence", GetActiveWindow()) == DialogResult::OK) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path relPath = fs::relative(filename, fs::current_path(), ec);
            std::string finalPath = (!ec && !relPath.empty()) ? relPath.generic_string() : filename;

            strcpy_s(g_SavePath, sizeof(g_SavePath), finalPath.c_str());

            std::ifstream is(finalPath);
            if (is.is_open()) {
                // ★修正: nlohmann::json に切り替え、クラッシュを防ぐ try-catch を導入
                try {
                    nlohmann::json j;
                    is >> j;

                    // 古い Cereal フォーマット("AnimSequence"がある)でも、新フォーマットでも安全に読めるようにする
                    if (j.contains("AnimSequence")) {
                        j.at("AnimSequence").get_to(g_EditingSequence);
                    }
                    else {
                        j.get_to(g_EditingSequence);
                    }

                    for (int i = 0; i < (int)animations.size(); ++i) {
                        if (animations[i].m_Name == g_EditingSequence.targetAnimName) {
                            s_SelectedAnimIndex = i; break;
                        }
                    }
                    _sequencerImpl->targetSequence = &g_EditingSequence;
                    _sequencerImpl->UpdateDuration(g_EditingSequence.duration);
                    _sequencerImpl->SyncFromData();
                    _selectedEntry = -1;
                    _fitCurveView = true; // ロード時にビューをフィット

                    CCL_LOG_SUCCESS(LogCategory::Editor, "Sequence Loaded: %s", finalPath.c_str());
                }
                catch (const std::exception& e) {
                    // ★万が一フォーマットが壊れていても、エディタが落ちるのではなくエラーログを出す
                    CCL_LOG_ERROR(LogCategory::Editor, "Failed to load sequence JSON: %s", e.what());
                }
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
    if (ImGui::BeginCombo("Target Animation", animations[s_SelectedAnimIndex].m_Name.c_str())) {
        for (int i = 0; i < (int)animations.size(); ++i) {
            bool isSelected = (s_SelectedAnimIndex == i);
            if (ImGui::Selectable(animations[i].m_Name.c_str(), isSelected)) s_SelectedAnimIndex = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (prevSelectedIndex != s_SelectedAnimIndex) {
        prevSelectedIndex = s_SelectedAnimIndex;
        g_EditingSequence.targetAnimName = animations[s_SelectedAnimIndex].m_Name;
        g_EditingSequence.duration = animations[s_SelectedAnimIndex].m_SecondsLength;
        _sequencerImpl->targetSequence = &g_EditingSequence;
        _sequencerImpl->UpdateDuration(g_EditingSequence.duration);
        _sequencerImpl->SyncFromData();
        _fitCurveView = true; // アニメ切り替え時にビューをフィット
    }

    // =========================================================
    // Edit Mode の制御
    // =========================================================
    if (context.isAnimEditMode) {
        animator->isEditorOverride = true;
        animator->currentSequence = &g_EditingSequence;
    }
    else {
        animator->isEditorOverride = false;
    }

    ImGui::Separator();

    // =========================================================
    // 2. メインレイアウト (左右2ペイン構造)
    // =========================================================
    if (ImGui::BeginTable("SequencerLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch, 0.75f);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableNextRow();

        // -----------------------------------------------------
        // 左カラム: タイムラインと追加ボタン
        // -----------------------------------------------------
        ImGui::TableSetColumnIndex(0);

        static bool s_isPlaying = false;
        static bool s_wasEditMode = false;

        // ショートカットキー（Spaceで再生/停止）
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Space)) {
            s_isPlaying = !s_isPlaying;
        }

        if (context.isAnimEditMode) {
            if (!s_wasEditMode) {
                context.animEditTime = animator->currentTimer;
                s_wasEditMode = true;
            }

            if (s_isPlaying) {
                float dt = ImGui::GetIO().DeltaTime;
                context.animEditTime += dt * animator->playbackSpeed;
                if (context.animEditTime > g_EditingSequence.duration) {
                    context.animEditTime = animator->isLoop
                        ? fmod(context.animEditTime, g_EditingSequence.duration)
                        : g_EditingSequence.duration;
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
        if (ImGui::Button("+ BB Flag")) { AddAndSelectEvent(3); }

        int currentFrame = context.isAnimEditMode ? (int)(context.animEditTime * 60.0f) : (int)(animator->currentTimer * 60.0f);
        int prevFrame = currentFrame;

        if (_sequencerImpl->targetSequence) {
            _expanded = true;
            bool changed = ImSequencer::Sequencer(_sequencerImpl.get(), &currentFrame, &_expanded, &_selectedEntry, &_firstFrame,
                ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_CHANGE_FRAME);

            if (changed) _sequencerImpl->SyncToData();

            //  Deleteキーで選択中のイベントブロックを削除
            if (_selectedEntry >= 0 && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                _sequencerImpl->Del(_selectedEntry);
                _sequencerImpl->SyncToData();
                _selectedEntry = -1;
            }

            if (currentFrame != prevFrame) {
                s_isPlaying = false;
                float newTime = (float)currentFrame / 60.0f;
                context.animEditTime = newTime;
                animator->currentTimer = newTime;
            }

            if (context.isAnimEditMode && context.systemManager) {
                if (auto* animSystem = context.systemManager->GetSystem<AnimationSystem>()) {
                    animSystem->UpdateManual(entity, context.animEditTime);
                    if (auto* transform = context.world->GetComponent<TransformComponent>(entity)) {
                        model->UpdateTransform(transform->worldMatrix);
                    }
                }
            }
        }

        // -----------------------------------------------------
        // 右カラム: カーブエディタ + インスペクタ
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

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Hit Stop Settings");

                // ヒットストップ時間のスライダー (floatParam1 を使用)
                if (ImGui::SliderFloat("Duration (sec)", &selectedItem.floatParam1, 0.0f, 1.0f, "%.2f s")) {
                    propChanged = true;
                }

                // ヒットストップ中の時間の進み具合 (floatParam2 を使用。0.0 で完全停止)
                if (ImGui::SliderFloat("Time Scale", &selectedItem.floatParam2, 0.0f, 1.0f, "%.2f x")) {
                    propChanged = true;
                }

            }
            else if (selectedItem.type == 1 || selectedItem.type == 2) {
                ImGui::Text(selectedItem.type == 1 ? "Audio File Path:" : "Prefab File Path:");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##FilePath", selectedItem.stringParam, sizeof(selectedItem.stringParam))) propChanged = true;

                if (ImGui::Button("Browse...", ImVec2(-1, 0))) {
                    const char* filter = (selectedItem.type == 1)
                        ? "Audio Files\0*.wav;*.mp3;*.ogg\0All Files\0*.*\0"
                        : "Prefab Files\0*.json\0All Files\0*.*\0";
                    char filename[256] = {};
                    if (Dialog::OpenFileName(filename, 256, filter, "Select File",
                        "Data/Animations/Sequence", GetActiveWindow()) == DialogResult::OK) {
                        namespace fs = std::filesystem;
                        std::error_code ec;
                        std::string finalPath = fs::relative(filename, fs::current_path(), ec).generic_string();
                        strcpy_s(selectedItem.stringParam, sizeof(selectedItem.stringParam), finalPath.c_str());
                        propChanged = true;
                    }
                }
            }
            else if (selectedItem.type == 3) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Blackboard Flag Settings");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20.0f);

                ImGui::Text("Quick Presets:");
                auto PresetButton = [&](const char* label, const char* flagName, ImVec4 color) {
                    ImGui::PushStyleColor(ImGuiCol_Button, color);
                    if (ImGui::Button(label)) {
                        strcpy_s(selectedItem.stringParam, sizeof(selectedItem.stringParam), flagName);
                        propChanged = true;
                    }
                    ImGui::PopStyleColor();
                    };

                PresetButton("Accept Input", "AcceptInput", ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                ImGui::SameLine();
                PresetButton("Attack Next", "AttackNext", ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::SameLine();
                PresetButton("Invincible", "IsInvincible", ImVec4(0.6f, 0.6f, 0.1f, 1.0f));

                ImGui::Spacing();
                ImGui::Separator();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
                if (ImGui::InputText("Flag Name", selectedItem.stringParam, sizeof(selectedItem.stringParam))) {
                    propChanged = true;
                }

                ImGui::BeginChild("BB_Preview", ImVec2(0, 60), true);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Generated Events:");
                ImGui::BulletText("Frame %d: BB_%s_True", selectedItem.frameStart, selectedItem.stringParam);
                ImGui::BulletText("Frame %d: BB_%s_False", selectedItem.frameEnd, selectedItem.stringParam);
                ImGui::EndChild();
            }

            if (propChanged) _sequencerImpl->SyncToData();
        }
        else {
            ImGui::TextDisabled("No event block selected.\nClick a block on the timeline.");
        }

        // =====================================================
        // ★改良版 カーブエディタ
        // =====================================================
        if (_sequencerImpl->targetSequence) {

            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "◆ Animation Curves");

            // --- ツールバー行1: Y軸レンジ ---
            ImGui::SetNextItemWidth(130);
            float yRange[2] = { _curveDelegate->m_min.y, _curveDelegate->m_max.y };
            if (ImGui::DragFloat2("Y-Range", yRange, 0.05f)) {
                _curveDelegate->m_min.y = yRange[0];
                _curveDelegate->m_max.y = yRange[1];
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("カーブの値(Y軸)の表示範囲");

            ImGui::SameLine();

            // --- ツールバー行1: X軸レンジ (Durationを超えて拡張可能) ---
            ImGui::SetNextItemWidth(130);
            float xRange[2] = { _curveDelegate->m_min.x, _curveDelegate->m_max.x };
            if (ImGui::DragFloat2("X-Range", xRange, 0.01f, 0.0f, 100.0f)) {
                if (xRange[1] > xRange[0] + 0.01f) {
                    _curveDelegate->m_min.x = xRange[0];
                    _curveDelegate->m_max.x = xRange[1];
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("時間(X軸)の表示範囲\nDurationを超えて広げることができます");

            ImGui::SameLine();

            // --- Fit ボタン: Durationに合わせてリセット ---
            if (ImGui::Button("Fit")) {
                _fitCurveView = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("アニメーションの長さに合わせてX軸をリセット");

            // --- ツールバー行2: カーブの表示/非表示 ---
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            ImGui::Checkbox("Speed", &_curveDelegate->visible[0]);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 0.65f, 1.0f, 1.0f));
            ImGui::Checkbox("RootMotion", &_curveDelegate->visible[1]);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("[Scroll:Pan | Ctrl+Scroll:Zoom]");

            // --- _fitCurveView フラグが立っていたらビューをリセット ---
            if (_fitCurveView && _curveDelegate->targetSeq) {
                _curveDelegate->FitViewToDuration();
                _fitCurveView = false;
            }

            _curveDelegate->SetSequence(&g_EditingSequence);

            // --- カーブエディタ本体を描画 ---
            ImVec2 curveCanvasPos = ImGui::GetCursorScreenPos();
            ImVec2 availRegion = ImGui::GetContentRegionAvail();
            const float CURVE_H = 260.0f;
            ImVec2 curveCanvasSize = ImVec2(availRegion.x, CURVE_H);

            // ★変更点: 先に背景グリッドとルーラーを描画する（下敷き）
            DrawCurveEditorOverlay(
                ImGui::GetWindowDrawList(),
                curveCanvasPos,
                curveCanvasSize,
                _curveDelegate->m_min,
                _curveDelegate->m_max,
                _curveDelegate->animDuration
            );

            // ★変更点: その上に、背景を透明にした ImCurveEdit を重ねて描画する
            ImCurveEdit::Edit(*_curveDelegate, curveCanvasSize, 999);

            // ★ マウスホイールでパン / Ctrl+ホイールでズーム
            if (ImGui::IsMouseHoveringRect(curveCanvasPos,
                ImVec2(curveCanvasPos.x + curveCanvasSize.x,
                    curveCanvasPos.y + curveCanvasSize.y)))
            {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    if (ImGui::GetIO().KeyCtrl) {
                        // --- Ctrl + ホイール: マウス位置を中心にズーム ---
                        const float mouseNX = (ImGui::GetIO().MousePos.x - curveCanvasPos.x) / curveCanvasSize.x;
                        const float focusT = _curveDelegate->m_min.x + mouseNX * (_curveDelegate->m_max.x - _curveDelegate->m_min.x);
                        const float zoomFactor = (wheel > 0.0f) ? 0.82f : 1.22f;
                        const float newRange = (_curveDelegate->m_max.x - _curveDelegate->m_min.x) * zoomFactor;
                        _curveDelegate->m_min.x = focusT - mouseNX * newRange;
                        _curveDelegate->m_max.x = _curveDelegate->m_min.x + newRange;
                    }
                    else {
                        // --- ホイール: X軸パン ---
                        const float panStep = (_curveDelegate->m_max.x - _curveDelegate->m_min.x) * 0.12f * -wheel;
                        _curveDelegate->m_min.x += panStep;
                        _curveDelegate->m_max.x += panStep;
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
        }


        ImGui::EndTable();
    }
}