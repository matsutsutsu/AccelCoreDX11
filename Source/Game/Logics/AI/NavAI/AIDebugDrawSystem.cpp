#include "AIDebugDrawSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include "Engine/Platform/Logger.h"

// ECS実行順序のマクロ
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// 数学ライブラリ
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;
using namespace CCL::ECS;

// ===================================================================================
// 【 AI可視化システム : AIDebugDrawSystem 】
//
// [ 役割 ]
// AIの内部データ（視界、聴力、現在のステート）を画面上にグラフィカルに描画し、ロジックの正当性を証明する。
//
// [ ECSデータパイプライン ]
// 📥 READ  : TransformComponent      (AIの位置と向き)
// 📥 READ  : AIPerceptionComponent   (視界の角度・距離、聴覚の半径)
// 📥 READ  : AIStateComponent        (現在のステートによる色分け用)
// 🚫 WRITE : なし (※描画システムはデータに一切の副作用を与えてはならない)
//
// [ 内部挙動の直感的な解説 ]
// RenderStage で動作し、ShapeRenderer を用いてデバッグ用の線や図形をオーバーレイ描画します。
// ・ステート: 頭上のスフィアの色（緑=巡回、黄=調査、赤=追跡）
// ・聴力: 地面に這うマゼンタの円
// ・視力: 正面を向くオレンジ色の扇形（コーン）
// ECSのViewを用いて連続メモリを高速に舐めるため、多数のAIがいてもゲームのフレームレートを落としません。
// ===================================================================================

void AIDebugDrawSystem::Update(float dt) {
    if (!isDebugVisible) return;

    // ShapeRenderer リソースの取得
    if (!_world || !_world->HasResource<ShapeRenderer*>()) return;
    auto renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    auto view = _world->View<TransformComponent, AIPerceptionComponent, AIStateComponent>();

    for (auto entity : view) {
        auto* transform = _world->GetComponent<TransformComponent>(entity);
        auto* perception = _world->GetComponent<AIPerceptionComponent>(entity);
        auto* state = _world->GetComponent<AIStateComponent>(entity);

        // ---------------------------------------------------------
        // 1. 基本パラメータの取得
        // ---------------------------------------------------------
        Vector3 pos = transform->position;
        Matrix worldMat = transform->worldMatrix;

        // ★修正: 視界が逆になる問題の解決
        // モデルの向きが-Zベースの場合、Backward()を使用するか、-Forward()とします。
        Vector3 forward = worldMat.Backward();
        forward.Normalize();

        // 目の高さ
        Vector3 eyePos = pos + Vector3(0.0f, 1.5f, 0.0f);

        // ---------------------------------------------------------
        // 2. ステートに応じたカラーコードの決定
        // ---------------------------------------------------------
        DirectX::XMFLOAT4 stateColor = { 0.5f, 0.5f, 0.5f, 1.0f }; // デフォルト: 灰色

        switch (state->currentState) {
        case AIState::Patrol:      stateColor = { 0.0f, 1.0f, 0.0f, 1.0f }; break; // 緑
        case AIState::Investigate: stateColor = { 1.0f, 1.0f, 0.0f, 1.0f }; break; // 黄
        case AIState::Chase:       stateColor = { 1.0f, 0.0f, 0.0f, 1.0f }; break; // 赤
        case AIState::AttackDoor:  stateColor = { 1.0f, 0.5f, 0.0f, 1.0f }; break; // オレンジ
        }

        // 頭上にステートの色を示す少し大きめの球を描画
        renderer->DrawSphere(eyePos + Vector3(0.0f, 0.6f, 0.0f), 0.3f, stateColor);


        // ---------------------------------------------------------
        // 4. 視覚コーンの描画 (正面から左右に広がる扇形)
        // ---------------------------------------------------------
        if (perception->visionRange > 0.0f && perception->visionAngle > 0.0f) {
            // ★修正: 目立つ強めの赤オレンジ色に変更
            DirectX::XMFLOAT4 visionColor = { 1.0f, 0.2f, 0.2f, 1.0f };

            float angleRad = DirectX::XMConvertToRadians(perception->visionAngle);

            // 左側の境界線ベクトル
            Matrix rotLeft = Matrix::CreateRotationY(-angleRad);
            Vector3 leftLimit = eyePos + (Vector3::TransformNormal(forward, rotLeft) * perception->visionRange);

            // 右側の境界線ベクトル
            Matrix rotRight = Matrix::CreateRotationY(angleRad);
            Vector3 rightLimit = eyePos + (Vector3::TransformNormal(forward, rotRight) * perception->visionRange);

            // 正面と左右の矢印
            Vector3 centerLimit = eyePos + (forward * perception->visionRange);
            renderer->DrawArrow(eyePos, centerLimit, 0.1f, visionColor);
            renderer->DrawArrow(eyePos, leftLimit, 0.1f, visionColor);
            renderer->DrawArrow(eyePos, rightLimit, 0.1f, visionColor);

            // ★追加: 視認性を劇的に上げるため、先端同士を線で結んで「扇形」の輪郭を作る
            if (renderer) { // DrawLine関数がShapeRendererにあることを前提とします
                // 滑らかな弧を描くために中間点も計算
                Vector3 midLeft = eyePos + (Vector3::TransformNormal(forward, Matrix::CreateRotationY(-angleRad * 0.5f)) * perception->visionRange);
                Vector3 midRight = eyePos + (Vector3::TransformNormal(forward, Matrix::CreateRotationY(angleRad * 0.5f)) * perception->visionRange);

                renderer->DrawLine(leftLimit, midLeft, visionColor);
                renderer->DrawLine(midLeft, centerLimit, visionColor);
                renderer->DrawLine(centerLimit, midRight, visionColor);
                renderer->DrawLine(midRight, rightLimit, visionColor);
            }
        }
    }
}

REGISTER_RENDER_SYSTEM(AIDebugDrawSystem, Priority::RenderStage::R08_Main)