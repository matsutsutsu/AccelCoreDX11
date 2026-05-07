#pragma once
#include <string>
#include <vector>
#include "Engine/Core/Math/StringHash.h"

// 1. 入力デバイスの種類
enum class InputDeviceType {
    Keyboard,         // キーボード
    Mouse_Button,     // マウスボタン
    GamePad_Button,   // ゲームパッドのボタン
    GamePad_AxisLX,   // 左スティックX
    GamePad_AxisLY,   // 左スティックY
    GamePad_AxisRX,   // 右スティックX
    GamePad_AxisRY,   // 右スティックY
    GamePad_TriggerL, // 左トリガー
    GamePad_TriggerR  // 右トリガー
};

// 2. マッピング（割り当て）の情報
struct InputBinding {
    InputDeviceType deviceType;
    unsigned int    keyCode;    // キーボードのVK_Wや、GamePad::BTN_Aなどのビットフラグ
    float           scale = 1.0f; // 軸の方向や感度（Wなら1.0、Sなら-1.0等）
};

// 3. 純粋な入力API
class IInputAPI {
public:
    virtual ~IInputAPI() = default;

    // エディタからホットリロードを叩けるようにインターフェースにも公開
    virtual bool LoadConfig(const std::string& filePath) = 0;

    // --- 設定（バインディング） ---
    virtual void AddActionBinding(const std::string& actionName, InputDeviceType device, unsigned int keyCode) = 0;
    virtual void AddAxisBinding(const std::string& axisName, InputDeviceType device, unsigned int keyCode, float scale = 1.0f) = 0;

    // =======================================================
    // ★ デジタル・アナログ入力の取得（コア機能：超高速版）
    // =======================================================
    virtual bool GetAction(uint32_t actionHash) const = 0;
    virtual bool GetActionTriggered(uint32_t actionHash) const = 0;
    virtual bool GetActionReleased(uint32_t actionHash) const = 0;
    virtual float GetAxis(uint32_t axisHash) const = 0;

    // =======================================================
    // ★ 既存コードを壊さないための安全なヘルパー関数
    // （純粋仮想関数 '= 0' ではなく、中身を直接ここに書きます）
    // =======================================================
    bool GetAction(const char* actionName) const { 
        return GetAction(CCL::Utils::HashString(actionName)); 
    }
    bool GetActionTriggered(const char* actionName) const { 
        return GetActionTriggered(CCL::Utils::HashString(actionName)); 
    }
    bool GetActionReleased(const char* actionName) const { 
        return GetActionReleased(CCL::Utils::HashString(actionName)); 
    }
    float GetAxis(const char* axisName) const { 
        return GetAxis(CCL::Utils::HashString(axisName)); 
    }
};