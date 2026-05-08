#pragma once
#include <string>
#include "Engine/UI/UIElement.h"
#include "UICommonEvents.h"
#include <map>

//すべてのイベントを記憶
void RegisterAllUIEvent();

// T は発行したいイベント構造体 (例: OnGameStartRequest)
class EventButton : public UIElement
{
private:
    // OnEditor() などの仮想関数が正しく動作するようにします
    std::vector<std::shared_ptr<UIEventCommon>> m_eventPayloads;
    bool m_isClicked = false;

    float m_hoverScaleMultiplier = 1.1f;
    float m_pressScaleMultiplier = 0.9f;

    // --- 型登録用の static メンバ ---
    // 型名 (string) と 生成関数 (lambda) のマップ
    using EventCreator = std::function<std::shared_ptr<UIEventCommon>()>;
    inline static std::map<std::string, EventCreator> s_eventRegistry;
public:
    // ゲーム起動時などに使用する型を登録しておくための関数
    template <typename T>
    static void RegisterEventType(const std::string& typeName) 
    {
        s_eventRegistry[typeName] = []() { return std::make_shared<T>(); };
    }

    // 名前からイベントを生成して追加する
    void AddEventByName(const std::string& typeName) {
        if (s_eventRegistry.count(typeName)) {
            m_eventPayloads.push_back(s_eventRegistry[typeName]());
        }
    }

    EventButton(const std::string& name, const std::string& spriteName)
        : UIElement(name, spriteName) {
    }

    // イベントデータを追加する（任意の構造体を push できる）
    template <typename T>
    void AddEventData(const T& data) {
        m_eventPayloads.push_back(std::make_shared<T>(data));
    }

    void OnClick() override {
        UIElement::OnClick();
        m_isClicked = true; // 押されたフラグを立てる
    }

    // Systemが回収するためのメソッド
    bool IsClicked() const { return m_isClicked; }

    void ClearClickFlag() { m_isClicked = false; }

    // --- インスペクター表示 ---
    void OnDebugGUI() override;
    // Systemが中身を取り出すためのゲッター
    const std::vector<std::shared_ptr<UIEventCommon>>& GetPayloads() const { return m_eventPayloads; }
};