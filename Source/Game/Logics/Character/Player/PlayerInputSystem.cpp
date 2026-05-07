#include "PlayerInputSystem.h"

#include "ECS/Core/CCL_Chunk.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Input/Input.h"
#include <SimpleMath.h>
#include <imgui.h>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// 必要に応じてWindows.hをインクルードしてください
// #define NOMINMAX
// #include <Windows.h>

using namespace CCL::ECS;
using namespace DirectX::SimpleMath; // Vector3, Matrix等を使用
using namespace DirectX;             // Vector3, Matrix等を使用

PlayerInputSystem::PlayerInputSystem()
    : IfSystem("PlayerInputSystem")
{
}

void PlayerInputSystem::Update(float dt)
{
    // 1. 世界から Input データ (Resource) を受け取る
    if (!_world->HasResource<Input*>()) return;
    auto* input = _world->GetResource<Input*>();
    if (!input) return;

    auto& pad = input->GetGamePad();
    auto& mouse = input->GetMouse();
    // Keyboardクラスも取得する
    auto& kb = input->GetKeyboard();



    // --- 追加: ゲーム状態のチェック ---
    if (_world->HasResource<GameState>()) {
        const auto &state = _world->GetResource<GameState>();

        // ゲームプレイ中以外は入力をゼロにして強制リターン（またはゼロ埋め処理）
        if (state.mode != GameStateMode::Gameplay) {
            ForEach([&](PlayerComponent &players) {
                players.input.moveDir = {0, 0, 0};
                players.input.jump    = false;
                players.input.attack  = false;
                // lookPosは更新しない、またはプレイヤーの正面を入れておく
            });
            return;
        }
    }


    // Logic用の蓄積入力を取得
    unsigned int logicPadDown   = input->GetPadButtonDownLogic();
    unsigned int logicMouseDown = input->GetMouseButtonDownLogic();

   

    ForEach([&](PlayerComponent &players) {
        // --- 移動入力の同時押し対策 ---
        float moveX = 0.0f;
        float moveZ = 0.0f;


        // （※あなたの Keyboard クラスの押しっぱなし判定関数が IsDown() であると仮定しています。
        //    もし IsPress() や GetKey() など名前が違う場合は、それに合わせてください）
        bool keyA = kb.IsDown('A');
        bool keyD = kb.IsDown('D');
        bool keyW = kb.IsDown('W');
        bool keyS = kb.IsDown('S');

        // X軸 (AとD)
        if (keyA && keyD) {
            moveX = 0.0f; // 両方押されていたら相殺して0にする
        }
        else if (keyA) {
            moveX = -1.0f;
        }
        else if (keyD) {
            moveX = 1.0f;
        }
        else {
            // キーボード入力がない場合は、パッド(スティック)の値を採用
            moveX = pad.GetAxisLX();
        }

        // Z軸 (WとS) も同様に処理
        if (keyW && keyS) {
            moveZ = 0.0f;
        }
        else if (keyW) {
            moveZ = 1.0f; // 前
        }
        else if (keyS) {
            moveZ = -1.0f; // 後ろ
        }
        else {
            moveZ = pad.GetAxisLY();
        }

        // 生入力を「ゲーム上の意味（アクション）」にマッピング
        // ここを書き換えるだけでキーコンフィグやマウス対応が可能になる
        players.input.moveDir.x = moveX;
        players.input.moveDir.y = 0.0f;
        players.input.moveDir.z = moveZ;

        players.input.jump = (logicPadDown & GamePad::BTN_A);

    
    });
}

REGISTER_LOGIC_SYSTEM(PlayerInputSystem, Priority::LogicStage::L01_Input);