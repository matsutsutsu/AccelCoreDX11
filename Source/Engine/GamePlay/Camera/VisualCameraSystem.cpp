#include "VisualCameraSystem.h"

// 実装で必要なものをここで初めてインクルードする
#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Input/Input.h"
#include <SimpleMath.h>
#include <cstdlib>
#include <imgui.h>


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
    // 1. スライダーをつかんでいる、テキスト入力中など「アイテムを操作中」か？
    bool isItemActive = ImGui::IsAnyItemActive();

    // 2. ImGuiのウィンドウ自体（背景やタイトルバー）を「クリック」しているか？
    bool isWindowClicked = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsAnyMouseDown();

    // ★ ウィンドウに重なっているだけ(ホバー)なら無視してカメラを動かす！
    if (isItemActive || isWindowClicked) {
        return; // UIを「操作」している時だけカメラを止める
    }

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
    // 1. スライダーをつかんでいる、テキスト入力中など「アイテムを操作中」か？
    bool isItemActive = ImGui::IsAnyItemActive();

    // 2. ImGuiのウィンドウ自体（背景やタイトルバー）を「クリック」しているか？
    bool isWindowClicked = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsAnyMouseDown();

    // ★ ウィンドウに重なっているだけ(ホバー)なら無視してカメラを動かす！
    if (isItemActive || isWindowClicked) {
        return; // UIを「操作」している時だけカメラを止める
    }

    if (!_world->HasResource<Input*>()) return;
    auto* input = _world->GetResource<Input*>();

    float lookX = input->GetGamePad().GetAxisRX() + input->GetMouse().GetVelocityX() * 0.1f;
    float lookY = input->GetGamePad().GetAxisRY() + input->GetMouse().GetVelocityY() * 0.1f;

    ForEach([&](VirtualCamera& vcam, CameraBodyTPS& tps, TransformComponent& trans) {
        if (!_world->IsEntityValid(tps.targetEntity)) return;
        auto* targetTrans = _world->GetComponent<TransformComponent>(tps.targetEntity);
        if (!targetTrans) return;

        // 1. 角度の更新
        tps.currentYaw += -lookX * tps.lookSpeedX * dt;
        tps.currentPitch += -lookY * tps.lookSpeedY * dt;
        tps.currentPitch = std::clamp(tps.currentPitch, -85.0f, 85.0f);

        float yawRad = XMConvertToRadians(tps.currentYaw);
        float pitchRad = XMConvertToRadians(tps.currentPitch);

        // 2. 目標方向 (Front) の計算
        Vector3 Front;
        Front.x = cos(yawRad) * cos(pitchRad);
        Front.y = sin(pitchRad);
        Front.z = sin(yawRad) * cos(pitchRad);
        Front.Normalize();

        // 3. 目標位置の計算
        Vector3 targetPos = targetTrans->position + Vector3(tps.targetOffset);
        // カメラは注視点から「Front方向」と逆に離れた位置にある
        Vector3 desiredPos = targetPos - (Front * tps.distance);

        // 4. 滑らかな補間 (解除時のジャンプを防ぐ)
        // ロックオンシステムが書き換えた trans.position から desiredPos へ滑らかに移動
        float lerpFactor = 1.0f - std::exp(-10.0f * dt);
        trans.position = Vector3::Lerp(trans.position, desiredPos, lerpFactor);

        // 回転の同期
        XMMATRIX lookMat = XMMatrixLookToLH(XMVectorZero(), Front, Vector3(0, 1, 0));
        XMVECTOR det;
        XMMATRIX worldRot = XMMatrixInverse(&det, lookMat);
        Quaternion desiredRot = Quaternion::CreateFromRotationMatrix(worldRot);
        trans.rotation = Quaternion::Slerp(trans.rotation, desiredRot, lerpFactor);

        vcam.resultPos = trans.position;
        vcam.resultLookAt = Vector3::Lerp(vcam.resultLookAt, targetPos, lerpFactor);
        vcam.isValid = true;
        });
}

void CameraFPSControlSystem::Update(float dt)
{
    // 1. スライダーをつかんでいる、テキスト入力中など「アイテムを操作中」か？
    bool isItemActive = ImGui::IsAnyItemActive();

    // 2. ImGuiのウィンドウ自体（背景やタイトルバー）を「クリック」しているか？
    bool isWindowClicked = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsAnyMouseDown();

    // ★ ウィンドウに重なっているだけ(ホバー)なら無視してカメラを動かす！
    if (isItemActive || isWindowClicked) {
        return; // UIを「操作」している時だけカメラを止める
    }

    if (!_world->HasResource<Input*>()) return;
    auto* input = _world->GetResource<Input*>();

    // 入力取得（TPSの方式に準拠）
    float lookX = input->GetGamePad().GetAxisRX() + input->GetMouse().GetVelocityX() * 0.1f;
    float lookY = -input->GetGamePad().GetAxisRY() + input->GetMouse().GetVelocityY() * 0.1f;

    ForEach([&](VirtualCamera& vcam, CameraBodyFPS& fps, TransformComponent& trans)
        {

            if (!_world->IsEntityValid(fps.targetEntity)) return;
            auto* targetTrans = _world->GetComponent<TransformComponent>(fps.targetEntity);
            if (!targetTrans) return;

            // --- 1. 回転の更新 ---
            fps.currentYaw += lookX * fps.mouseSensitivity * 100.0f * dt;
            fps.currentPitch += lookY * fps.mouseSensitivity * 100.0f * dt;
            fps.currentPitch = std::clamp(fps.currentPitch, fps.minPitch, fps.maxPitch);

            float yawRad = XMConvertToRadians(fps.currentYaw);
            float pitchRad = XMConvertToRadians(fps.currentPitch);

            XMVECTOR baseRotQuat = XMQuaternionRotationRollPitchYaw(pitchRad, yawRad, 0);
            XMVECTOR offsetRotQuat = XMLoadFloat4(&fps.rotationOffset);
            XMVECTOR targetRotQuat = XMQuaternionMultiply(offsetRotQuat, baseRotQuat);

            // --- 2. 補間係数の計算 (TPSと同じ方式) ---
            // 指数減衰を用いたフレームレート独立な補間
            float lerpFactor = 1.0f - std::exp(-15.0f * dt); // 15.0fは追従の鋭さ。好みで10.0f〜20.0fで調整

            // 回転の補間 (Slerp)
            trans.rotation = Quaternion::Slerp(trans.rotation, targetRotQuat, lerpFactor);

            // --- 3. 座標の更新 ---
            Vector3 desiredEyePos = Vector3(targetTrans->position) + Vector3(fps.eyeOffset);

            // 座標の補間 (TPSと同じ Lerp)
            trans.position = Vector3::Lerp(trans.position, desiredEyePos, lerpFactor);

            // --- 4. VirtualCamera への書き込み ---
            XMVECTOR finalRotQuat = XMLoadFloat4(&trans.rotation);
            XMVECTOR forwardVec = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), finalRotQuat);
            XMVECTOR upVec = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), finalRotQuat);

            vcam.resultPos = trans.position;
            XMStoreFloat3(&vcam.resultLookAt, XMLoadFloat3(&trans.position) + forwardVec);
            XMStoreFloat3(&vcam.resultUp, upVec);

            vcam.isValid = true;
        });
}

void CameraLockOnSystem::Update(float dt)
{
    ForEach([&](VirtualCamera& vcam, 
        CameraBodyTPS& tps, 
        CameraLockOn& Targeting, 
        TransformComponent& trans)
        {
            bool hasValidTarget = _world->IsEntityValid(Targeting.targetEntity) && _world->IsEntityValid(tps.targetEntity);

            if (!hasValidTarget) {
                Targeting.isInitialized = false;
                if (Targeting.originalLookSpeedX >= 0.0f) {
                    tps.lookSpeedX = Targeting.originalLookSpeedX;
                    tps.lookSpeedY = Targeting.originalLookSpeedY;
                    if (Targeting.originalDistance >= 0.0f) tps.distance = Targeting.originalDistance;
                    Targeting.originalLookSpeedX = -1.0f;
                }
                return;
            }

            if (Targeting.originalLookSpeedX < 0.0f) {
                Targeting.originalLookSpeedX = tps.lookSpeedX;
                Targeting.originalLookSpeedY = tps.lookSpeedY;
                Targeting.originalDistance = tps.distance;
                tps.lookSpeedX = 0.0f;
                tps.lookSpeedY = 0.0f;
            }

            auto* enemyTrans = _world->GetComponent<TransformComponent>(Targeting.targetEntity);
            auto* playerTrans = _world->GetComponent<TransformComponent>(tps.targetEntity);
            if (!enemyTrans || !playerTrans) return;

            // --- 1. 目標位置の計算 ---
            Vector3 pPos = Vector3(playerTrans->position) + Vector3(tps.targetOffset);
            Vector3 ePos = Vector3(enemyTrans->position) + Vector3(Targeting.targetOffset);
            Vector3 dirPE = pPos - ePos; // 敵からプレイヤーへの方向
            float distPE = dirPE.Length();
            

            // ★追加：距離による強制解除
            if (distPE > Targeting.maxDistance) {
                Targeting.targetEntity = CCL::ECS::InvalidEntityID; // ターゲットを無効化
                return; // 次のフレームの開始時に解除ロジックが走る
            }

            dirPE.Normalize();

            Vector3 worldUp = Vector3(0, 1, 0);
            Vector3 sideVec = worldUp.Cross(dirPE);
            sideVec.Normalize();

            // 理想的なカメラ位置
            Vector3 cameraPosGoal = pPos + (dirPE * tps.distance) + (sideVec * Targeting.sideOffset * Targeting.currentSide);
            if (cameraPosGoal.y < pPos.y) cameraPosGoal.y = pPos.y;

            // 理想的な注視点
            float proximityScale = std::clamp(1.0f - (distPE / Targeting.maxDistance), 0.0f, 1.0f);
            float dynamicFocus = std::lerp(0.3f, 0.5f, proximityScale);
            Vector3 lookAtGoal = Vector3::Lerp(pPos, ePos, dynamicFocus);

            // --- 2. 補間処理 ---
            if (!Targeting.isInitialized || Targeting.lastTargetEntity != Targeting.targetEntity) {
                Targeting.currentInterpolatedPos = trans.position;
                Targeting.currentInterpolatedLookAt = vcam.resultLookAt;
                Targeting.isInitialized = true;
                Targeting.lastTargetEntity = Targeting.targetEntity;
            }

            float lerpFactor = std::clamp(Targeting.interpolationSpeed * dt, 0.0f, 1.0f);
            Targeting.currentInterpolatedPos = Vector3::Lerp(Targeting.currentInterpolatedPos, cameraPosGoal, lerpFactor);
            Targeting.currentInterpolatedLookAt = Vector3::Lerp(Targeting.currentInterpolatedLookAt, lookAtGoal, lerpFactor);

            // --- 3. 反映と角度の逆算 ---
            Matrix rotationMatrix = Matrix::CreateLookAt(Targeting.currentInterpolatedPos, Targeting.currentInterpolatedLookAt, worldUp).Invert();
            trans.position = Targeting.currentInterpolatedPos;
            trans.rotation = Quaternion::CreateFromRotationMatrix(rotationMatrix);

            // ★重要：ここが反転対策のキモ
            // 現在の rotation から「カメラが向いている正面ベクトル」を取り出す
            Vector3 forward = Vector3::Transform(Vector3(0, 0, 1), trans.rotation);
            forward.Normalize();

            // TPS側の座標系 (x=cos, z=sin) に合わせて Yaw/Pitch を同期
            tps.currentPitch = XMConvertToDegrees(asin(forward.y));
            float yaw = XMConvertToDegrees(atan2(forward.z, forward.x)) + 180.0f;

            // 角度を -180 ~ 180 または 0 ~ 360 の範囲に丸める（念のため）
            if (yaw > 180.0f) yaw -= 360.0f;
            if (yaw < -180.0f) yaw += 360.0f;

            tps.currentYaw = yaw;

            vcam.resultPos = trans.position;
            vcam.resultLookAt = Targeting.currentInterpolatedLookAt;
            vcam.isValid = true;
        });
}

// システムの登録
// ファイル末尾に追加
REGISTER_LOGIC_SYSTEM(CameraTPSControlSystem, Priority::LogicStage::L02_Update);
REGISTER_LOGIC_SYSTEM(CameraFPSControlSystem, Priority::LogicStage::L02_Update);
REGISTER_LOGIC_SYSTEM(CameraLockOnSystem, Priority::LogicStage::L02_Update);

// =========================================================
// 自動登録マクロ群（必ず .cpp の末尾に1回だけ書く）
// =========================================================
REGISTER_RENDER_SYSTEM(CameraFollowSystem, Priority::RenderStage::R01_Camera);
REGISTER_RENDER_SYSTEM(CameraLookAtSystem, Priority::RenderStage::R01_Camera);
REGISTER_RENDER_SYSTEM(CameraShakeSystem, Priority::RenderStage::R01_Camera);
REGISTER_RENDER_SYSTEM(CameraFreeControlSystem, Priority::RenderStage::R01_Camera);


