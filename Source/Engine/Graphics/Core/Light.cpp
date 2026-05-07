#include "Light.h"
#include "GpuResourceUtils.h"
#include <imgui.h>
#include <string>

// 初期化：定数バッファの作成
void LightManager::Initialize(ID3D11Device* device)
{
	// 定数バッファ作成
    HRESULT hr = GpuResourceUtils::CreateDynamicConstantBuffer(
		device,
		sizeof(LightConstantBuffer),
		constantBuffer.GetAddressOf());

    if (FAILED(hr)) {
        // ここでエラーが出ていないか！？
        OutputDebugStringA("FAILED TO CREATE LIGHT CONSTANT BUFFER\n");
    }
}




// CPU上のデータをGPUの定数バッファに転送し、パイプラインにバインドする
void LightManager::Bind(ID3D11DeviceContext* dc, int slot)
{
    if (!constantBuffer || slot < 0) return;

    // 1. vectorのデータを定数バッファ用の固定長配列にコピー（詰め替え）
    cbData.pointLightCount = static_cast<int>(pointLights.size());
    for (int i = 0; i < cbData.pointLightCount; ++i)
    {
        cbData.pointLights[i] = pointLights[i];
    }

    cbData.spotLightCount = static_cast<int>(spotLights.size());
    for (int i = 0; i < cbData.spotLightCount; ++i)
    {
        cbData.spotLights[i] = spotLights[i];
    }

    // 2. GPUメモリへの転送
    // D3D11_USAGE_DYNAMIC で作成されていることを想定し Map/Unmap を使用
	// UpdateResourceと違って高速に動作する
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = dc->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &cbData, sizeof(LightConstantBuffer));
        dc->Unmap(constantBuffer.Get(), 0);
    }
    else 
    {
          // Mapに失敗しているなら、バッファの作成フラグ（USAGE）が間違っている可能性がある
          OutputDebugStringA("MAP FAILED\n");
    }

    // // Map/Unmapを捨てて、最も確実な転送方法を試す  定数バッファ作成をDEFAULTにした場合使う
    //dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cbData, 0, 0);

    // 3. PSSetConstantBuffers の引数は (slot, 数, バッファの配列)
    ID3D11Buffer* buffers[] = { constantBuffer.Get() };
    // CPU側でこのデータをGPUのslot番号「〇番」に置いてくれと命令するだけ
    // Hlsliファイルを知る必要はない
    dc->PSSetConstantBuffers((UINT)slot, 1, buffers);
}

// GUI描画
void LightManager::DrawGUI()
{
    if (ImGui::CollapsingHeader(u8"Light Manager (ライト管理)"))
    {
        // --- Ambient Light (環境光/半球ライティング) ---
        if (ImGui::TreeNode(u8"Ambient Light (環境光 - 半球ライティング)"))
        {
            // gSkyColor に対応
            ImGui::ColorEdit4(u8"skyColor (空の色/強度)", &cbData.dirLight.skyColor.x);
            // gGroundColor に対応
            ImGui::ColorEdit4(u8"groundColor (地面の色/強度)", &cbData.dirLight.groundColor.x);

            ImGui::TreePop();
        }

        ImGui::Separator();



        ImGui::Separator();

        // --- Directional Light (平行光源) ---
        if (ImGui::TreeNode(u8"Directional Light (平行光源)"))
        {
            // gDirLight に対応
            ImGui::DragFloat3(u8"Direction (向き)", &cbData.dirLight.direction.x, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3(u8"Color (色)", &cbData.dirLight.color.x);
            ImGui::DragFloat(u8"Intensity (強度)", &cbData.dirLight.intensity, 0.01f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        ImGui::Separator();

        // --- Point Lights (点光源) ---
        if (ImGui::TreeNode(u8"Point Lights (点光源)"))
        {
            if (ImGui::Button(u8"Add Point Light (点光源を追加)") && pointLights.size() < MAX_POINT_LIGHTS)
            {
                pointLights.push_back(PointLights{});
            }

            for (size_t i = 0; i < pointLights.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));

                // active フラグの操作
                bool isActive = pointLights[i].active > 0.5f;
                if (ImGui::Checkbox(u8"##Active", &isActive)) {
                    pointLights[i].active = isActive ? 1.0f : 0.0f;
                }
                ImGui::SameLine();

                std::string label = u8"Point Light (点光源) " + std::to_string(i);
                if (ImGui::TreeNode(label.c_str()))
                {
                    // 各変数名と対応
                    ImGui::DragFloat3(u8"position (座標)", &pointLights[i].position.x, 0.1f);
                    ImGui::DragFloat(u8"range (範囲)", &pointLights[i].range, 0.1f, 0.0f, 100.0f);
                    ImGui::ColorEdit3(u8"color (色)", &pointLights[i].color.x);
                    ImGui::DragFloat(u8"intensity (強度)", &pointLights[i].intensity, 0.05f, 0.0f, 20.0f);

                    if (ImGui::TreeNode(u8"Attenuation (減衰設定)"))
                    {
                        ImGui::SliderFloat(u8"constant (定数減衰)", &pointLights[i].constantAttenuation, 0.0f, 2.0f);
                        ImGui::SliderFloat(u8"linear (線形減衰)", &pointLights[i].linearAttenuation, 0.0f, 1.0f);
                        ImGui::SliderFloat(u8"quadratic (二次減衰)", &pointLights[i].quadraticAttenuation, 0.0f, 0.5f);
                        ImGui::TreePop();
                    }

                    if (ImGui::Button(u8"Remove (削除)"))
                    {
                        pointLights.erase(pointLights.begin() + i);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        ImGui::Separator();

        // --- Spot Lights (スポットライト) ---
        if (ImGui::TreeNode(u8"Spot Lights (スポットライト)"))
        {
            if (ImGui::Button(u8"Add Spot Light (追加)") && spotLights.size() < MAX_SPOT_LIGHTS)
            {
                spotLights.push_back(SpotLights{});
            }

            for (size_t i = 0; i < spotLights.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i + 100));

                bool isActive = spotLights[i].active > 0.5f;
                if (ImGui::Checkbox(u8"##Active", &isActive)) {
                    spotLights[i].active = isActive ? 1.0f : 0.0f;
                }
                ImGui::SameLine();

                std::string label = u8"Spot Light (スポット) " + std::to_string(i);
                if (ImGui::TreeNode(label.c_str()))
                {
                    ImGui::DragFloat3(u8"position (座標)", &spotLights[i].position.x, 0.1f);
                    ImGui::DragFloat3(u8"direction (向き)", &spotLights[i].direction.x, 0.01f, -1.0f, 1.0f);
                    ImGui::DragFloat(u8"range (範囲)", &spotLights[i].range, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat(u8"intensity (強度)", &spotLights[i].intensity, 0.05f, 0.0f, 20.0f);

                    ImGui::Separator();
                    ImGui::SliderFloat(u8"innerCos (内角)", &spotLights[i].innerCos, 0.0f, 1.0f);
                    ImGui::SliderFloat(u8"outerCos (外角)", &spotLights[i].outerCos, 0.0f, 1.0f);
                    ImGui::ColorEdit3(u8"color (色)", &spotLights[i].color.x);

                    if (ImGui::TreeNode(u8"Attenuation (減衰設定)"))
                    {
                        ImGui::SliderFloat(u8"constant", &spotLights[i].constantAttenuation, 0.0f, 2.0f);
                        ImGui::SliderFloat(u8"linear", &spotLights[i].linearAttenuation, 0.0f, 1.0f);
                        ImGui::SliderFloat(u8"quadratic", &spotLights[i].quadraticAttenuation, 0.0f, 0.5f);
                        ImGui::TreePop();
                    }

                    if (ImGui::Button(u8"Remove (削除)"))
                    {
                        spotLights.erase(spotLights.begin() + i);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}