#pragma once
#include "Editor/Core/EditorWindow.h"
#include <vector>
#include <string>

class InputEditorWindow : public EditorWindow {
public:
    InputEditorWindow();
    virtual ~InputEditorWindow() = default;

protected:
    void DrawContents(EditorContext& context) override;

private:
    void LoadFromJson();
    void SaveToJson();

    // 内部データ構造 (JSONと1対1で対応するエディタ用データ)
    struct BindingData {
        std::string device = "Keyboard";
        std::string key = "NONE";
        float scale = 1.0f;
    };

    struct ActionData {
        std::string name;
        std::vector<BindingData> bindings;
    };

    struct AxisData {
        std::string name;
        std::vector<BindingData> bindings;
    };

    std::vector<ActionData> _actions;
    std::vector<AxisData> _axes;

    std::string _configPath = "Assets/Config/InputConfig.json";
};