// Engine/UI/UISystem.cpp
#include "UISystem.h"
#include "Engine/UI/UIManager.h"
#include "Engine/Platform/Input/Input.h"

#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/UI/UIContext.h"

#include "Game/UI/PlayerHUD.h"
#include"Game/UI/UIButton.h"

using namespace CCL::ECS;

UISystem::UISystem() : SystemBase("UISystem")
{
    // 特定のコンポーネントをフィルタリングしない場合は空にするか、
    // UIをコンポーネント化している場合はそのフィルタを設定します。
}

std::vector<TypeID> UISystem::GetReadTypes() const
{
    // UIManagerリソースが必要であることを示す（必要に応じて定義）
    return {};
}

void UISystem::Initialize()
{
    // 1. UIManagerリソースを取得
    if (!_world->HasResource<UIManager*>()) return;
    auto* uiMgr = _world->GetResource<UIManager*>();
}

void UISystem::Update(float dt)
{
    // 1. リソースから UIManager を取得
    if (!_world->HasResource<UIManager*>()) return;
    UIManager* uiMgr = _world->GetResource<UIManager*>();

    // 2. マウス入力の取得と翻訳 (SceneGame からの移植)
    auto& mouseInput = Input::Instance().GetMouse();

    UIMouseState uiMouse;
    uiMouse.x = (float)mouseInput.GetPositionX();
    uiMouse.y = (float)mouseInput.GetPositionY();

    uiMouse.isDown = mouseInput.IsDown(Mouse::BTN_LEFT);
    uiMouse.isPressed = mouseInput.IsTriggered(Mouse::BTN_LEFT);
    uiMouse.isReleased = mouseInput.IsReleased(Mouse::BTN_LEFT);

    // 3. UIの更新
    uiMgr->Update(dt, uiMouse);


    RenderUI();
}

// ボタンのイベントを回収して EventBus に投げる専用メソッド
void UISystem::ProcessUIEvents(UIManager* uiMgr)
{
    // UIManager から全ての UI 要素を取得 (再帰的にリスト化されている前提)
    auto elements = uiMgr->GetAllElements();
    auto& bus = _world->GetEventBus(); // あなたの EventBus インスタンス

    for (auto& element : elements) {
        auto button = std::dynamic_pointer_cast<EventButton>(element);
        if (button && button->IsClicked()) {
            for (auto& eventData : button->GetPayloads()) {
                // UISystemは中身を知らなくても、Emitを呼ぶだけで適切なイベントが飛びます
                eventData->Emit(bus);
            }
            button->ClearClickFlag();
        }
    }
}

void UISystem::RenderUI()
{
    // 1. UIManagerリソースを取得
    if (!_world->HasResource<UIManager*>()) return;
    auto* uiMgr = _world->GetResource<UIManager*>();

    uiMgr->Render();       // HUD含むUI
}


// 実行順序の登録。UIはゲームロジックの後、描画の直前が理想的です。
REGISTER_RENDER_SYSTEM(UISystem, Priority::RenderStage::R10_UI);