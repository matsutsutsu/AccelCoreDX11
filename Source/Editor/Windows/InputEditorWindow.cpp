#include "InputEditorWindow.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include <imgui.h>
#include <fstream>
#include <json.hpp>

using json = nlohmann::json;

InputEditorWindow::InputEditorWindow() : EditorWindow("Input Mapping Editor")
{
    // ウィンドウ起動時にとりあえず読み込んでおく
    LoadFromJson();
}

void InputEditorWindow::LoadFromJson()
{
    _actions.clear();
    _axes.clear();

    std::ifstream file(_configPath);
    if (!file.is_open()) return;

    json doc;
    try {
        file >> doc;

        if (doc.contains("Actions")) {
            for (const auto& act : doc["Actions"]) {
                ActionData action;
                action.name = act["Name"].get<std::string>();
                for (const auto& b : act["Bindings"]) {
                    action.bindings.push_back({ b["Device"], b["Key"], 1.0f });
                }
                _actions.push_back(action);
            }
        }

        if (doc.contains("Axes")) {
            for (const auto& ax : doc["Axes"]) {
                AxisData axis;
                axis.name = ax["Name"].get<std::string>();
                for (const auto& b : ax["Bindings"]) {
                    axis.bindings.push_back({ b["Device"], b["Key"], b.value("Scale", 1.0f) });
                }
                _axes.push_back(axis);
            }
        }
    }
    catch (...) {
        // パースエラー時は何もしない
    }
}

void InputEditorWindow::SaveToJson()
{
    json doc;
    doc["Actions"] = json::array();
    doc["Axes"] = json::array();

    for (const auto& act : _actions) {
        json jAct;
        jAct["Name"] = act.name;
        jAct["Bindings"] = json::array();
        for (const auto& b : act.bindings) {
            jAct["Bindings"].push_back({ {"Device", b.device}, {"Key", b.key} });
        }
        doc["Actions"].push_back(jAct);
    }

    for (const auto& ax : _axes) {
        json jAx;
        jAx["Name"] = ax.name;
        jAx["Bindings"] = json::array();
        for (const auto& b : ax.bindings) {
            jAx["Bindings"].push_back({ {"Device", b.device}, {"Key", b.key}, {"Scale", b.scale} });
        }
        doc["Axes"].push_back(jAx);
    }

    std::ofstream file(_configPath);
    if (file.is_open()) {
        file << doc.dump(4); // インデント付きで綺麗に保存
    }
}

void InputEditorWindow::DrawContents(EditorContext& context)
{
    // -------------------------------------------------------------
    // 上部ツールバー：保存とホットリロード
    // -------------------------------------------------------------
    if (ImGui::Button("Load from JSON")) {
        LoadFromJson();
    }
    ImGui::SameLine();

    // ★ ここがゲーム開発用ツールの要：「保存して即座にエンジンへ反映」
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("Save & Apply to Engine")) {
        SaveToJson();

        // エンジンのInputFacadeにホットリロード命令を出す
        if (context.world && context.world->HasResource<std::shared_ptr<IInputAPI>>()) {
            auto inputAPI = context.world->GetResource<std::shared_ptr<IInputAPI>>();
            inputAPI->LoadConfig(_configPath);
        }
    }
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // デバイスの選択肢
    const char* devices[] = {
        "Keyboard", "Mouse_Button",
        "GamePad_Button", "GamePad_AxisLX", "GamePad_AxisLY",
        "GamePad_AxisRX", "GamePad_AxisRY", "GamePad_TriggerL", "GamePad_TriggerR"
    };

    // --- キーリストの定義 ---
    // staticにすることで毎フレームの配列生成を避けます
    static std::vector<const char*> keyList = {
        "NONE", "SPACE", "LSHIFT", "RSHIFT", "UP", "DOWN", "LEFT", "RIGHT",
        "W", "A", "S", "D", // よく使うものを上に
        "BTN_LEFT", "BTN_RIGHT", "BTN_A", "BTN_B", "BTN_X", "BTN_Y",
        "BTN_RIGHT_TRIGGER", "BTN_LEFT_TRIGGER"
    };

    // 初回実行時に A-Z を自動追加（手動で書く手間を省く場合）
    static bool isKeyListInitialized = false;
    if (!isKeyListInitialized) {
        // AからZまでを順番に追加（既存のW,A,S,Dと重複しないようにチェック）
        for (char c = 'A'; c <= 'Z'; ++c) {
            std::string s(1, c);
            bool exists = false;
            for (const char* k : keyList) {
                if (std::string(k) == s) { exists = true; break; }
            }
            if (!exists) {
                // 注意: 文字列リテラルではないため、寿命を管理する必要があります。
                // ここでは簡易化のため new していますが、本来は static な std::string のリストを持つのが安全です。
                char* dynamicKey = new char[2];
                dynamicKey[0] = c;
                dynamicKey[1] = '\0';
                keyList.push_back(dynamicKey);
            }
        }
        isKeyListInitialized = true;
    }


    auto DrawBindingParams = [&](BindingData& b, bool isAxis) {
        // Device コンボボックス
        if (ImGui::BeginCombo("Device", b.device.c_str())) {
            for (const char* dev : devices) {
                bool isSelected = (b.device == dev);
                if (ImGui::Selectable(dev, isSelected)) b.device = dev;
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Key コンボボックス（カスタム文字も打てるように InputText にしても良いが、今回はコンボ）
        if (ImGui::BeginCombo("Key", b.key.c_str())) {
            for (const char* k : keyList) {
                bool isSelected = (b.key == k);
                if (ImGui::Selectable(k, isSelected)) b.key = k;
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // AxisのみScaleを表示
        if (isAxis) {
            ImGui::DragFloat("Scale", &b.scale, 0.1f, -1.0f, 1.0f);
        }
        };

    // -------------------------------------------------------------
    // タブによる Action と Axis の切り替え
    // -------------------------------------------------------------
    if (ImGui::BeginTabBar("InputTabs")) {

        // ==========================================
        // Actions (単発ボタン)
        // ==========================================
        if (ImGui::BeginTabItem("Actions (Digital)")) {
            if (ImGui::Button("+ Add New Action")) {
                _actions.push_back({ "NewAction", {} });
            }
            ImGui::Separator();

            for (size_t i = 0; i < _actions.size(); ++i) {
                ImGui::PushID((int)i);
                auto& act = _actions[i];

                char nameBuf[64];
                strcpy_s(nameBuf, act.name.c_str());
                if (ImGui::InputText("Action Name", nameBuf, sizeof(nameBuf))) {
                    act.name = nameBuf;
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 100);
                if (ImGui::Button("Delete Action")) {
                    _actions.erase(_actions.begin() + i);
                    ImGui::PopID();
                    break;
                }

                ImGui::Indent();
                for (size_t j = 0; j < act.bindings.size(); ++j) {
                    ImGui::PushID((int)j);
                    ImGui::Text("Bind %zu:", j);
                    ImGui::SameLine();
                    if (ImGui::Button("x Delete")) {
                        act.bindings.erase(act.bindings.begin() + j);
                        ImGui::PopID();
                        break;
                    }
                    DrawBindingParams(act.bindings[j], false);
                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (ImGui::Button("+ Add Binding")) {
                    act.bindings.push_back(BindingData());
                }
                ImGui::Unindent();
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        // ==========================================
        // Axes (アナログ軸)
        // ==========================================
        if (ImGui::BeginTabItem("Axes (Analog)")) {
            if (ImGui::Button("+ Add New Axis")) {
                _axes.push_back({ "NewAxis", {} });
            }
            ImGui::Separator();

            for (size_t i = 0; i < _axes.size(); ++i) {
                ImGui::PushID((int)i);
                auto& ax = _axes[i];

                char nameBuf[64];
                strcpy_s(nameBuf, ax.name.c_str());
                if (ImGui::InputText("Axis Name", nameBuf, sizeof(nameBuf))) {
                    ax.name = nameBuf;
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 100);
                if (ImGui::Button("Delete Axis")) {
                    _axes.erase(_axes.begin() + i);
                    ImGui::PopID();
                    break;
                }

                ImGui::Indent();
                for (size_t j = 0; j < ax.bindings.size(); ++j) {
                    ImGui::PushID((int)j);
                    ImGui::Text("Bind %zu:", j);
                    ImGui::SameLine();
                    if (ImGui::Button("x Delete")) {
                        ax.bindings.erase(ax.bindings.begin() + j);
                        ImGui::PopID();
                        break;
                    }
                    DrawBindingParams(ax.bindings[j], true);
                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (ImGui::Button("+ Add Binding")) {
                    ax.bindings.push_back(BindingData());
                }
                ImGui::Unindent();
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}