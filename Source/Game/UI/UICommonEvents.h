#pragma once

#include <string>

//すべてのUI関係のイベントの基礎
struct UIEventCommon
{
    virtual ~UIEventCommon() = default;
    // インスペクターで中身をいじるための仮想関数
    virtual void OnEditor() {}
};

// 汎用テンプレート変数の指定をするときに使います
template <typename T>
struct UIValueEvent : public UIEventCommon 
{
    T value;
    UIValueEvent(T v) : value(v) {}
};


struct SceneChangeRequest : public UIEventCommon 
{
    std::string scenePath;

    void OnEditor() override;
};

