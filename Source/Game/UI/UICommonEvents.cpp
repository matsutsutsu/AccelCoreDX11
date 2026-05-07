#include "UICommonEvents.h"
#include "ImGui.h"



void SceneChangeRequest::OnEditor()
{
    char buf[256];
    strcpy_s(buf, scenePath.c_str());
    if (ImGui::InputText("Scene Path", buf, sizeof(buf))) 
    {
        scenePath = buf; // インスペクターから直接パスを書き換えられる！
    }
}
