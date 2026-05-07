#pragma once
#include "IInputAPI.h"
#include "Input.h"
#include <unordered_map>
#include <string>

class InputFacade : public IInputAPI {
private:
    Input* m_rawInput; // 生のデバイス入力基盤

    // 辞書データ（アクション名 -> 複数の割り当てリスト）
    std::unordered_map<uint32_t, std::vector<InputBinding>> m_actionBindings;
    std::unordered_map<uint32_t, std::vector<InputBinding>> m_axisBindings;

public:
    InputFacade(Input* rawInput) : m_rawInput(rawInput) {}
    virtual ~InputFacade() = default;

    // 外部JSONファイルからバインディングを読み込む
    bool LoadConfig(const std::string& filePath) override;

    void AddActionBinding(const std::string& actionName, InputDeviceType device, unsigned int keyCode) override;
    void AddAxisBinding(const std::string& axisName, InputDeviceType device, unsigned int keyCode, float scale = 1.0f) override;

    // オーバーライドする関数を uint32_t に合わせる
    bool GetAction(uint32_t actionHash) const override;
    bool GetActionTriggered(uint32_t actionHash) const override;
    bool GetActionReleased(uint32_t actionHash) const override;
    float GetAxis(uint32_t axisHash) const override;
};