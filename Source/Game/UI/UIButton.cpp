#include "UIButton.h"

#include "ImGui.h"


void RegisterAllUIEvent()
{
    EventButton::RegisterEventType<SceneChangeRequest>("Scene Change");
}

void EventButton::OnDebugGUI() 
{
    // 1. アニメーション設定
    ImGui::DragFloat("Hover Scale", &m_hoverScaleMultiplier, 0.01f);
    ImGui::DragFloat("Press Scale", &m_pressScaleMultiplier, 0.01f);

    ImGui::Separator();
    ImGui::Text("Events");

    // 2. 既存のペイロードの表示と編集
    for (int i = 0; i < (int)m_eventPayloads.size(); ++i) 
    {
        auto& ev = m_eventPayloads[i];
        ImGui::PushID(i);

        if (ImGui::TreeNode(typeid(*ev).name()))
        {
            ev->OnEditor(); // 各イベント独自の編集UIを表示

            if (ImGui::Button("Remove Event"))
            {
                m_eventPayloads.erase(m_eventPayloads.begin() + i);
                ImGui::TreePop();
                ImGui::PopID();
                break; // ループを抜けて次フレームで更新
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();

    // 3. 新しいイベントを動的に追加するUI
    if (ImGui::BeginCombo("Add New Event", "Select Type...")) 
    {
        for (auto const& [name, creator] : s_eventRegistry) {
            if (ImGui::Selectable(name.c_str())) {
                AddEventByName(name);
            }
        }
        ImGui::EndCombo();
    }
}

