#include "ImGuiThemes.h"
#include <imgui.h>

void ImGuiThemes::ApplyLight()
{
    ImGui::StyleColorsLight();
}


void ImGuiThemes::ApplyDark()
{
    ImGui::StyleColorsDark();
}


void ImGuiThemes::ApplyClassic()
{
    ImGui::StyleColorsClassic();
}

// Darcula 風 (IntelliJ IDEA 系)
void ImGuiThemes::ApplyDarcula()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.23f, 0.23f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.33f, 0.33f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
}

void ImGuiThemes::ApplySkeltonDarcula()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    float SkeltonAlpha = 0.75f;

    style.Alpha = SkeltonAlpha;

    colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.23f, 0.23f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.33f, 0.33f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
}


// Unity Editor 風
void ImGuiThemes::ApplyUnityDark()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.176f, 0.176f, 0.176f, 1.000f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
}

// Visual Studio Dark 風
void ImGuiThemes::ApplyVisualStudioDark()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.105f, 0.105f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
}
