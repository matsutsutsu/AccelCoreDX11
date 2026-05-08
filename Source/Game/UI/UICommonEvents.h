#pragma once
#include "ECS/Core/CCL_EventBus.h"
#include <string>


//すべてのUI関係のイベントの基礎
struct UIEventCommon
{
    virtual ~UIEventCommon() = default;
    // インスペクターで中身をいじるための仮想関数
    virtual void OnEditor() {}
    // 自分を適切な型として EventBus に投げるための仮想関数
    virtual void Emit(CCL::ECS::Core::EventBus& bus) = 0;
};

// 汎用テンプレート変数の指定をするときに使います
template <typename T>
struct UIValueEvent : public UIEventCommon
{
    T value;
    UIValueEvent(T v) : value(v) {}
    T GetValue() { return value; }
};


struct SceneChangeRequest : public UIEventCommon
{
    std::string scenePath;

    void OnEditor() override;
    void Emit(CCL::ECS::Core::EventBus& bus) override {
        // 自分自身を具体的な型として Publish する
        bus.Publish(*this);
    }
    std::string GetScenePath() { return scenePath; }
};

