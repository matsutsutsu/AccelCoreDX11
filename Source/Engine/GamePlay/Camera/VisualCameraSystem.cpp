#include "VisualCameraSystem.h"

// 実装で必要なものをここで初めてインクルードする
#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Input/Input.h"
#include <SimpleMath.h>
#include <cstdlib>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// =========================================================
// 1. CameraFollowSystem
// =========================================================
void CameraFollowSystem::Update(float dt)
{
    ForEach([&](VirtualCamera &vcam, const CameraBodyFollow &body) {
        // ターゲットのTransformを取得
        auto *targetTrans = _world->GetComponent<TransformComponent>(body.target);
        if (!targetTrans) return;

        // 目標位置の計算 (TransformComponentにGetWorldPositionがない場合は .position
        // を直接使用) XMFLOAT3 worldPos = targetTrans->GetWorldPosition();
        XMFLOAT3 worldPos  = targetTrans->position;
        XMVECTOR targetPos = XMLoadFloat3(&worldPos);
        XMVECTOR offset    = XMLoadFloat3(&body.offset);

        // シンプルな加算（本来はターゲットの回転を考慮して回すべきだが、まずは固定オフセットで）
        XMVECTOR desiredPos = targetPos + offset;

        // Y軸固定オプション
        if (body.lockY) {
            // Yだけ現在のvCamの高さを維持（あるいは固定値）
            // 実装省略：必要ならここで desiredPos のYを書き換え
        }

        // 初期化フレーム対策（まだ計算されていないならワープ）
        if (!vcam.isValid) {
            XMStoreFloat3(&vcam.resultPos, desiredPos);
            return;
        }

        // 現在位置からの補間 (Damping)
        XMVECTOR currentPos = XMLoadFloat3(&vcam.resultPos);
        // Lerp係数: 1.0 - exp(-damping * dt) がフレームレート非依存の正しい減衰計算
        float    t      = 1.0f - std::exp(-body.damping * dt);
        XMVECTOR newPos = XMVectorLerp(currentPos, desiredPos, t);

        XMStoreFloat3(&vcam.resultPos, newPos);
    });
}

// =========================================================
// 2. CameraLookAtSystem
// =========================================================
void CameraLookAtSystem::Update(float dt)
{
    ForEach([&](VirtualCamera &vcam, const CameraAimLookAt &aim) {
        auto *targetTrans = _world->GetComponent<TransformComponent>(aim.target);
        if (!targetTrans) return;

        // XMFLOAT3 worldPos    = targetTrans->GetWorldPosition();
        XMFLOAT3 worldPos    = targetTrans->position;
        XMVECTOR targetPos   = XMLoadFloat3(&worldPos);
        XMVECTOR offset      = XMLoadFloat3(&aim.offset);
        XMVECTOR desiredLook = targetPos + offset;

        // 初期化フレーム対策
        if (!vcam.isValid) {
            XMStoreFloat3(&vcam.resultLookAt, desiredLook);
            // 有効フラグを立てる（PositionとLookAt両方計算済みとする）
            vcam.isValid = true;
            return;
        }

        // 現在のLookAtからの補間
        XMVECTOR currentLook = XMLoadFloat3(&vcam.resultLookAt);
        float    t           = 1.0f - std::exp(-aim.damping * dt);
        XMVECTOR newLook     = XMVectorLerp(currentLook, desiredLook, t);

        XMStoreFloat3(&vcam.resultLookAt, newLook);
    });
}

// =========================================================
// 3. CameraShakeSystem
// =========================================================
void CameraShakeSystem::Update(float dt)
{
    ForEach([&](VirtualCamera &vcam, CameraShake &shake) {
        // シェイク終了なら何もしない
        if (shake.duration <= 0.0f) return;

        // 1. 減衰計算 (Trauma Curve)
        // 時間経過とともに 1.0 -> 0.0 になる係数
        float trauma = shake.duration / shake.maxDuration;

        // 2乗することで「最初は激しく、すぐ落ち着く」自然な揺れにする
        // 線形(traumaそのまま)だと機械的な揺れに見えるため
        float shakePower = trauma * trauma * shake.amplitude;

        // 2. ランダムオフセット生成 (-1.0 ~ 1.0 * power)
        // 簡易的なrand()を使用。より高品質にするならPerlinNoiseを使う。
        float rx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakePower;
        float ry = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakePower;
        float rz = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakePower;

        // 3. 計算済みの結果に加算 (Position)
        XMVECTOR pos = XMLoadFloat3(&vcam.resultPos);
        pos += XMVectorSet(rx, ry, rz, 0);
        XMStoreFloat3(&vcam.resultPos, pos);

        // 4. 計算済みの結果に加算 (LookAt)
        // 注視点も少しずらすと、より「衝撃」感が出る
        XMVECTOR look = XMLoadFloat3(&vcam.resultLookAt);
        look += XMVectorSet(rx * 0.5f, ry * 0.5f, 0, 0); // Z軸はあまり揺らさない
        XMStoreFloat3(&vcam.resultLookAt, look);

        // 5. 時間経過
        shake.duration -= dt;
        if (shake.duration < 0.0f) shake.duration = 0.0f;
    });
}

// =========================================================
// 4. CameraFreeControlSystem
// =========================================================
void CameraFreeControlSystem::Update(float dt)
{
    // シングルトンから入力取得
    auto &input = Input::Instance();
    auto &mouse = input.GetMouse();
    auto &kb    = input.GetKeyboard();

    using namespace DirectX;

    // マウスデルタ
    float deltaX     = static_cast<float>(mouse.GetPositionX() - mouse.GetOldPositionX());
    float deltaY     = static_cast<float>(mouse.GetPositionY() - mouse.GetOldPositionY());
    int   wheelDelta = mouse.GetWheel();

    ForEach([&](VirtualCamera &vcam, CameraBodyFree &body, TransformComponent &trans) {
        // --- 1. 回転計算 (FreeCameraSystemと同じロジック) ---
        if (mouse.IsDown(Mouse::BTN_RIGHT)) {
            // 感度適用 (body.lookSpeedは度数法想定: 0.2f程度)
            body.currentYaw += deltaX * body.lookSpeed;
            body.currentPitch -= deltaY * body.lookSpeed;

            // ピッチ制限 (-89 ~ 89度)
            body.currentPitch = std::clamp(body.currentPitch, -89.9f, 89.9f);

            // 移動速度調整をここ（右クリック中）に移動する
            if (wheelDelta != 0) {
                body.moveSpeed *= (wheelDelta > 0) ? 1.2f : 0.8f;
                body.moveSpeed = std::clamp(body.moveSpeed, 0.1f, 1000.0f);
            }
        }

        // 度数 -> ラジアン変換
        float yawRad   = XMConvertToRadians(body.currentYaw);
        float pitchRad = XMConvertToRadians(body.currentPitch);

        // 前方ベクトル (Front) の計算: 球面座標系
        // これにより「常に水平を保ちつつ上下を向く」FPS挙動になります
        XMVECTOR Front = XMVector3Normalize(XMVectorSet(
            cosf(pitchRad) * sinf(yawRad), sinf(pitchRad), cosf(pitchRad) * cosf(yawRad), 0.0f));

        // 右ベクトル (Right) と 上ベクトル (Up) の計算
        XMVECTOR WorldUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR Right   = XMVector3Normalize(XMVector3Cross(WorldUp, Front));
        // カメラ固有の上方向（移動用にはWorldUpを使うことが多いが、回転同期のために計算）
        XMVECTOR Up = XMVector3Normalize(XMVector3Cross(Front, Right));

        //// --- 2. 移動速度調整 (ホイール) ---
        //if (wheelDelta != 0) {
        //    body.moveSpeed *= (wheelDelta > 0) ? 1.2f : 0.8f;
        //    body.moveSpeed = std::clamp(body.moveSpeed, 0.1f, 1000.0f);
        //}

        // --- 3. 移動計算 ---
        XMVECTOR Pos = XMLoadFloat3(&trans.position);
        float    s   = body.moveSpeed * dt;

        // FreeCameraComponentと同じ操作系
        if (kb.IsDown('W')) Pos += Front * s;
        if (kb.IsDown('S')) Pos -= Front * s;
        if (kb.IsDown('D')) Pos += Right * s;
        if (kb.IsDown('A')) Pos -= Right * s;
        if (kb.IsDown('E')) Pos += WorldUp * s; // 真上上昇
        if (kb.IsDown('Q')) Pos -= WorldUp * s; // 真下下降

        // 結果をTransformに書き戻す
        XMStoreFloat3(&trans.position, Pos);

        // 回転もTransformに反映させておく（デバッグ表示などで正しい向きに見えるように）
        // LookRotation (Front, Up) から Quaternion を作成
        // ※SimpleMathが使えるなら Quaternion::LookRotation(Front, Up) ですが、
        // ここでは汎用的にYaw/Pitchから生成します
        Quaternion qRot = Quaternion::CreateFromYawPitchRoll(yawRad, -pitchRad, 0.0f);
        // ※Pitchの符号は座標系によりますが、CreateFromYawPitchRollは通常正負が逆になることがあるため調整
        //  あるいは Frontベクトルから直接LookAt行列を作って分解するのが確実ですが、
        //  今回はvCamへの反映がメインなので簡易計算します。

        // FrontからLookAt行列を作り、そこからQuaternion抽出（これが一番確実）
        XMMATRIX lookMat = XMMatrixLookToLH(XMVectorZero(), Front, WorldUp);
        // LookToはView行列(逆行列)を作るので、Inverseしてワールド回転にする
        XMVECTOR det;
        XMMATRIX worldRot = XMMatrixInverse(&det, lookMat);
        trans.rotation    = Quaternion::CreateFromRotationMatrix(worldRot);

        // --- 4. VirtualCameraへの同期 ---
        // 現在の位置
        vcam.resultPos = trans.position;

        // 見ている先 = 現在位置 + 前方ベクトル
        // (注視点を遠くに置くことで回転を安定させる)
        vcam.resultLookAt = trans.position + (Vector3(Front) * 10.0f);

        // アップベクトル
        vcam.resultUp = Vector3(0, 1, 0);

        // 計算完了フラグ
        vcam.isValid = true;
    });
}

// =========================================================
// TPS: 三人称視点カメラ
// =========================================================
void CameraTPSControlSystem::Update(float dt)
{
    if (!_world->HasResource<Input*>()) return;
    auto* input = _world->GetResource<Input*>();

    // 入力（右スティック ＋ マウス）
    float lookX = input->GetGamePad().GetAxisRX() + input->GetMouse().GetVelocityX() * 0.1f;
    float lookY = input->GetGamePad().GetAxisRY() + input->GetMouse().GetVelocityY() * 0.1f;

    ForEach([&](VirtualCamera& vcam, CameraBodyTPS& tps, TransformComponent& trans) {

        // 1. ターゲットの取得
        if (!_world->IsEntityValid(tps.targetEntity)) return;
        auto* targetTrans = _world->GetComponent<TransformComponent>(tps.targetEntity);
        if (!targetTrans) return;

        // 2. 角度の更新
        tps.currentYaw += -lookX * tps.lookSpeedX * dt;
        tps.currentPitch += -lookY * tps.lookSpeedY * dt;

        // ジンバルロック防止のためPitchを制限
        tps.currentPitch = std::clamp(tps.currentPitch, -85.0f, 85.0f);

        float yawRad = XMConvertToRadians(tps.currentYaw);
        float pitchRad = XMConvertToRadians(tps.currentPitch);

        // 3. カメラの前方ベクトルを計算 (FreeCameraの方式に準拠)
        Vector3 Front;
        Front.x = cos(yawRad) * cos(pitchRad);
        Front.y = sin(pitchRad);
        Front.z = sin(yawRad) * cos(pitchRad);
        Front.Normalize();

        // 4. 座標と回転の計算
        Vector3 targetPos = targetTrans->position + Vector3(tps.targetOffset);
        trans.position = targetPos - (Front * tps.distance); // ターゲットの「後ろ」に配置

        XMMATRIX lookMat = XMMatrixLookToLH(XMVectorZero(), Front, Vector3(0, 1, 0));
        XMVECTOR det;
        XMMATRIX worldRot = XMMatrixInverse(&det, lookMat);
        trans.rotation = Quaternion::CreateFromRotationMatrix(worldRot);

        // 5. VirtualCameraへの同期
        vcam.resultPos = trans.position;
        vcam.resultLookAt = targetPos;
        vcam.resultUp = Vector3(0, 1, 0);
        vcam.isValid = true;
        });
}

// ファイル末尾に追加
REGISTER_LOGIC_SYSTEM(CameraTPSControlSystem, Priority::LogicStage::L02_Update);


// =========================================================
// 自動登録マクロ群（必ず .cpp の末尾に1回だけ書く）
// =========================================================
REGISTER_RENDER_SYSTEM(CameraFollowSystem, Priority::RenderStage::R01_Camera);
REGISTER_RENDER_SYSTEM(CameraLookAtSystem, Priority::RenderStage::R01_Camera);
REGISTER_RENDER_SYSTEM(CameraShakeSystem, Priority::RenderStage::R01_Camera);
REGISTER_RENDER_SYSTEM(CameraFreeControlSystem, Priority::RenderStage::R01_Camera);