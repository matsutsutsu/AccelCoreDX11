#pragma once
#include "Editor/Core/EditorWindow.h"
#include "Engine/GamePlay/Animation/Data/AnimationCurve.h"
#include <ImCurveEdit.h>
#include <string>
#include <algorithm> // std::clamp用

// ===================================================================================
// カーブデータと ImCurveEdit を繋ぐ通訳クラス
// ===================================================================================
struct CurveDelegate : public ImCurveEdit::Delegate {
    AnimationCurve* targetCurve;
    ImVec2 points[256];
    bool visible[1] = { true };

    // グラフの表示範囲（X: 0~1.0, Y: 0~3.0）
    ImVec2 m_min = ImVec2(0.0f, 0.0f);
    ImVec2 m_max = ImVec2(1.0f, 3.0f);

    CurveDelegate(AnimationCurve* curve) : targetCurve(curve) {
        UpdatePoints();
    }

    void UpdatePoints() {
        for (size_t i = 0; i < targetCurve->keys.size() && i < 256; ++i) {
            points[i] = ImVec2(targetCurve->keys[i].time, targetCurve->keys[i].value);
        }
    }

    // --- ImCurveEdit::Delegate のオーバーライド ---
    size_t GetCurveCount() override { return 1; }
    bool IsVisible(size_t curveIndex) override { return visible[curveIndex]; }
    size_t GetPointCount(size_t curveIndex) override { return targetCurve->keys.size(); }
    uint32_t GetCurveColor(size_t curveIndex) override { return 0xFF00FF00; }
    ImVec2* GetPoints(size_t curveIndex) override { return points; }

    // 不足していた純粋仮想関数の実装
    virtual ImVec2& GetMin() override { return m_min; }
    virtual ImVec2& GetMax() override { return m_max; }
    virtual unsigned int GetBackgroundColor() override { return 0xFF202020; }

    int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override {
        // X軸(時間)は絶対に 0.0 ~ 1.0 の範囲を越えられないように制限する
        value.x = std::clamp(value.x, 0.0f, 1.0f);

        targetCurve->keys[pointIndex].time = value.x;
        targetCurve->keys[pointIndex].value = value.y;
        UpdatePoints();
        return pointIndex;
    }

    void AddPoint(size_t curveIndex, ImVec2 value) override {
        CurveKey newKey = { value.x, value.y };
        targetCurve->keys.push_back(newKey);

        std::sort(targetCurve->keys.begin(), targetCurve->keys.end(),
            [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });

        UpdatePoints();
    }
};

class CurveEditorWindow : public EditorWindow {
public:
    CurveEditorWindow();
    virtual ~CurveEditorWindow() = default;

protected:
    void DrawContents(EditorContext& context) override;

private:
    AnimationCurve _editingCurve;
    std::string _currentFilePath = "Assets/Curves/NewCurve.json";

    void SaveCurve(const std::string& path);
    void LoadCurve(const std::string& path);
};