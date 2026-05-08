#include "UIEditorWindow.h"
#include "Engine/UI/UIManager.h"
#include "Engine/UI/UIElement.h"
// UIEditorWindow.cpp

void UIEditorWindow::DrawContents(EditorContext& context)
{
    if (!context.world->HasResource<UIManager*>()) return;
    auto* uiMgr = context.world->GetResource<UIManager*>();

    // 上部：基本情報
    ImGui::Text("Global Scale: %.2f", uiMgr->GetGlobalScale());
    ImGui::Separator();

    // 2カラムレイアウトの開始
    ImGui::Columns(2, "UIEditorColumns");

    // --- 左側：Hierarchy（階層構造） ---
    ImGui::TextDisabled("Hierarchy");
    ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), true);

    // UIManagerが持つルート要素からツリーを開始
    // ※UIManagerに rootElements のゲッターが必要
    for (auto& root : uiMgr->GetRootElements()) {
        DrawElementTree(root, uiMgr);
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    // --- 右側：Inspector（詳細編集） ---
    ImGui::TextDisabled("Inspector");
    ImGui::BeginChild("InspectorView", ImVec2(0, 0), true);

    // UIManagerで選択されている要素を表示
    auto selected = uiMgr->GetSelectedElement();
    if (selected) {
        // UIElementが既に持っているデバッグ関数を呼び出す
        // ただし、ツリーを二重にしないように「中身だけ」描画する形に調整
        selected->OnDebugGUI();
    }
    else {
        ImGui::Text("Select an element to edit.");
    }
    ImGui::EndChild();

    ImGui::Columns(1);
}

// 再帰的にツリーを描画するヘルパー
void UIEditorWindow::DrawElementTree(std::shared_ptr<UIElement> element, UIManager* uiMgr)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (uiMgr->GetSelectedElement() == element) flags |= ImGuiTreeNodeFlags_Selected;

    // 子要素がない場合は葉として扱う[cite: 6, 7]
    if (element->GetChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool nodeOpen = ImGui::TreeNodeEx(element.get(), flags, "%s", element->GetName().c_str());

    if (ImGui::IsItemClicked()) {
        uiMgr->SetSelectedElement(element);
    }

    if (nodeOpen && !element->GetChildren().empty()) {
        for (auto& child : element->GetChildren()) {
            DrawElementTree(child, uiMgr);
        }
        ImGui::TreePop();
    }
}
