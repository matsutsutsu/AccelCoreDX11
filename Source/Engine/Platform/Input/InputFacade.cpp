#include "InputFacade.h"
#include <algorithm>
#include <fstream>
#include <iostream>

// ※お使いのJSONライブラリに合わせてください
 #include <json.hpp>
 using json = nlohmann::json;
 
 namespace {
     // ---------------------------------------------------------
     // ヘルパー: 文字列 -> デバイスタイプ変換
     // ---------------------------------------------------------
     InputDeviceType StringToDeviceType(const std::string& str) {
         if (str == "Keyboard") return InputDeviceType::Keyboard;
         if (str == "Mouse_Button") return InputDeviceType::Mouse_Button;
         if (str == "GamePad_Button") return InputDeviceType::GamePad_Button;
         if (str == "GamePad_AxisLX") return InputDeviceType::GamePad_AxisLX;
         if (str == "GamePad_AxisLY") return InputDeviceType::GamePad_AxisLY;
         if (str == "GamePad_AxisRX") return InputDeviceType::GamePad_AxisRX;
         if (str == "GamePad_AxisRY") return InputDeviceType::GamePad_AxisRY;
         if (str == "GamePad_TriggerL") return InputDeviceType::GamePad_TriggerL;
         if (str == "GamePad_TriggerR") return InputDeviceType::GamePad_TriggerR;
         return InputDeviceType::Keyboard; // Default
     }

     // ---------------------------------------------------------
     // ヘルパー: 文字列 -> キーコード変換
     // ---------------------------------------------------------
     unsigned int StringToKeyCode(const std::string& str) {
         if (str == "NONE") return 0; // 軸入力などでキー判定が不要な場合

         // --- キーボード (アルファベットはASCIIコードそのまま) ---
         if (str.length() == 1 && str[0] >= 'A' && str[0] <= 'Z') return str[0];
         if (str == "SPACE") return VK_SPACE;
         if (str == "ESC")   return VK_ESCAPE;
         if (str == "UP")    return VK_UP;
         if (str == "DOWN")  return VK_DOWN;
         if (str == "LEFT")  return VK_LEFT;
         if (str == "RIGHT") return VK_RIGHT;

         // --- マウス ---
         if (str == "BTN_LEFT")   return Mouse::BTN_LEFT;
         if (str == "BTN_RIGHT")  return Mouse::BTN_RIGHT;
         if (str == "BTN_MIDDLE") return Mouse::BTN_MIDDLE;

         // --- ゲームパッド ---
         if (str == "BTN_A") return GamePad::BTN_A;
         if (str == "BTN_B") return GamePad::BTN_B;
         if (str == "BTN_X") return GamePad::BTN_X;
         if (str == "BTN_Y") return GamePad::BTN_Y;
         if (str == "BTN_RIGHT_TRIGGER")  return GamePad::BTN_RIGHT_TRIGGER;
         if (str == "BTN_LEFT_TRIGGER")   return GamePad::BTN_LEFT_TRIGGER;
         if (str == "BTN_RIGHT_SHOULDER") return GamePad::BTN_RIGHT_SHOULDER;
         if (str == "BTN_LEFT_SHOULDER")  return GamePad::BTN_LEFT_SHOULDER;

         // 未知のキーは一旦0を返す。必要に応じて追加してください
         return 0;
     }
 }

 // =======================================================
 // JSONファイルからのロード処理
 // =======================================================
 bool InputFacade::LoadConfig(const std::string& filePath) {
     std::ifstream file(filePath);
     if (!file.is_open()) {
         // ※エンジンのログシステムでエラーを出力してください
         // OutputDebugStringA("Failed to open InputConfig.json\n");
         return false;
     }

     // JSONライブラリによるパース (以下は nlohmann/json を想定した記述です)
     
     json doc;
     try {
         file >> doc;
     } catch (...) {
         return false;
     }

     // 既存のバインディングをクリア（再読み込みに対応するため）
     m_actionBindings.clear();
     m_axisBindings.clear();

     // 1. Actions の読み込み
     if (doc.contains("Actions")) {
         for (const auto& actionJson : doc["Actions"]) {
             std::string name = actionJson["Name"].get<std::string>();
             for (const auto& bindJson : actionJson["Bindings"]) {
                 InputDeviceType device = StringToDeviceType(bindJson["Device"].get<std::string>());
                 unsigned int keyCode = StringToKeyCode(bindJson["Key"].get<std::string>());

                 AddActionBinding(name, device, keyCode);
             }
         }
     }

     // 2. Axes の読み込み
     if (doc.contains("Axes")) {
         for (const auto& axisJson : doc["Axes"]) {
             std::string name = axisJson["Name"].get<std::string>();
             for (const auto& bindJson : axisJson["Bindings"]) {
                 InputDeviceType device = StringToDeviceType(bindJson["Device"].get<std::string>());
                 unsigned int keyCode = StringToKeyCode(bindJson["Key"].get<std::string>());
                 float scale = bindJson.value("Scale", 1.0f); // デフォルトは1.0f

                 AddAxisBinding(name, device, keyCode, scale);
             }
         }
     }
     

     return true;
 }

 // =======================================================
 // 辞書への登録 (文字列をハッシュ化して登録する)
 // =======================================================
 void InputFacade::AddActionBinding(const std::string& actionName, InputDeviceType device, unsigned int keyCode) {
     uint32_t hash = CCL::Utils::HashString(actionName);
     m_actionBindings[hash].push_back({ device, keyCode, 1.0f });
 }

 void InputFacade::AddAxisBinding(const std::string& axisName, InputDeviceType device, unsigned int keyCode, float scale) {
     uint32_t hash = CCL::Utils::HashString(axisName);
     m_axisBindings[hash].push_back({ device, keyCode, scale });
 }

 // =======================================================
 // デジタル入力（Action）の取得 (ハッシュで最速検索)
 // =======================================================
 bool InputFacade::GetAction(uint32_t actionHash) const {
     if (!m_rawInput) return false;
     auto it = m_actionBindings.find(actionHash);
     if (it == m_actionBindings.end()) return false;

     for (const auto& bind : it->second) {
         switch (bind.deviceType) {
         case InputDeviceType::Keyboard:       if (m_rawInput->GetKeyboard().IsDown(bind.keyCode)) return true; break;
         case InputDeviceType::Mouse_Button:   if (m_rawInput->GetMouse().IsDown(bind.keyCode)) return true; break;
         case InputDeviceType::GamePad_Button: if (m_rawInput->GetGamePad().IsDown(bind.keyCode)) return true; break;
         default: break;
         }
     }
     return false;
 }

 bool InputFacade::GetActionTriggered(uint32_t actionHash) const {
     if (!m_rawInput) return false;
     auto it = m_actionBindings.find(actionHash);
     if (it == m_actionBindings.end()) return false;

     for (const auto& bind : it->second) {
         switch (bind.deviceType) {
         case InputDeviceType::Keyboard:       if (m_rawInput->GetKeyboard().IsTriggered(bind.keyCode)) return true; break;
         case InputDeviceType::Mouse_Button:   if (m_rawInput->GetMouse().IsTriggered(bind.keyCode)) return true; break;
         case InputDeviceType::GamePad_Button: if (m_rawInput->GetGamePad().IsTriggered(bind.keyCode)) return true; break;
         default: break;
         }
     }
     return false;
 }

 bool InputFacade::GetActionReleased(uint32_t actionHash) const {
     if (!m_rawInput) return false;
     auto it = m_actionBindings.find(actionHash);
     if (it == m_actionBindings.end()) return false;

     for (const auto& bind : it->second) {
         switch (bind.deviceType) {
         case InputDeviceType::Keyboard:       if (m_rawInput->GetKeyboard().IsReleased(bind.keyCode)) return true; break;
         case InputDeviceType::Mouse_Button:   if (m_rawInput->GetMouse().IsReleased(bind.keyCode)) return true; break;
         case InputDeviceType::GamePad_Button: if (m_rawInput->GetGamePad().IsReleased(bind.keyCode)) return true; break;
         default: break;
         }
     }
     return false;
 }

 // =======================================================
 // アナログ入力（Axis）の取得 (ハッシュで最速検索)
 // =======================================================
 float InputFacade::GetAxis(uint32_t axisHash) const {
     if (!m_rawInput) return 0.0f;
     auto it = m_axisBindings.find(axisHash);
     if (it == m_axisBindings.end()) return 0.0f;

     float finalValue = 0.0f;

     for (const auto& bind : it->second) {
         float rawValue = 0.0f;

         switch (bind.deviceType) {
         case InputDeviceType::Keyboard:       if (m_rawInput->GetKeyboard().IsDown(bind.keyCode)) rawValue = 1.0f; break;
         case InputDeviceType::Mouse_Button:   if (m_rawInput->GetMouse().IsDown(bind.keyCode)) rawValue = 1.0f; break;
         case InputDeviceType::GamePad_Button: if (m_rawInput->GetGamePad().IsDown(bind.keyCode)) rawValue = 1.0f; break;
         case InputDeviceType::GamePad_AxisLX: rawValue = m_rawInput->GetGamePad().GetAxisLX(); break;
         case InputDeviceType::GamePad_AxisLY: rawValue = m_rawInput->GetGamePad().GetAxisLY(); break;
         case InputDeviceType::GamePad_AxisRX: rawValue = m_rawInput->GetGamePad().GetAxisRX(); break;
         case InputDeviceType::GamePad_AxisRY: rawValue = m_rawInput->GetGamePad().GetAxisRY(); break;
         case InputDeviceType::GamePad_TriggerL: rawValue = m_rawInput->GetGamePad().GetTriggerL(); break;
         case InputDeviceType::GamePad_TriggerR: rawValue = m_rawInput->GetGamePad().GetTriggerR(); break;
         }

         finalValue += rawValue * bind.scale;
     }

     return std::clamp(finalValue, -1.0f, 1.0f);
 }