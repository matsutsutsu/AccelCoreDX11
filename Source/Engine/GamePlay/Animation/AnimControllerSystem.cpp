#include "AnimControllerSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <cmath> 
#include "Engine/Graphics/Resource/ResourceManager.h"
#include "Engine/GamePlay/Animation/Data/AnimGraphSerializer.h"
#include "Engine/Platform/Logger.h"
#include <imgui.h>
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include <algorithm> // std::clamp用
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

void AnimControllerSystem::Update(float dt)
{
    auto& resourceManager = ResourceManager::Instance();

    ForEachWithIDParallel([&resourceManager](CCL::ECS::EntityID id,
        AnimStateMachineComponent& fsm,
        AnimatorComponent& animator,
        AnimParametersComponent& params,
        const TransformComponent& transform)
        {
            // エディタで操作中の場合は、ステートマシンからの再生命令（干渉）を完全に遮断する
            if (animator.isEditorOverride) return;

            // ========================================================================
            // 1. グラフのロードと初期化 (ポインタを使わず直接 internalGraph を操作する)
            // ========================================================================
            if (fsm.internalGraph.states.empty() && !fsm.graphPath.empty()) {
                if (AnimGraphSerializer::LoadFromJSON(fsm.internalGraph, fsm.graphPath, {})) {

                    for (auto& state : fsm.internalGraph.states) {
                        if (!state.sequenceFilePath.empty()) {
                            state.sequence = resourceManager.LoadAnimSequence(state.sequenceFilePath.c_str());
                        }
                    }

                    fsm.currentStateHash = fsm.internalGraph.entryStateHash; // 初期状態へ

                    // ★ 初期再生のブートストラップ
                    for (const auto& state : fsm.internalGraph.states) {
                        if (state.stateHash == fsm.currentStateHash) {
                            if (state.sequence) {
                                animator.Play(state.sequence, state.isLoop, state.playbackSpeed);
                            }
                            break;
                        }
                    }
                }
                else {
                    fsm.graphPath = ""; // ロード失敗時はクリア
                }
            }

            // グラフが空なら処理しない
            if (fsm.internalGraph.states.empty()) return;

            // ========================================================================
            // 2. 現在のステートを検索 (internalGraph を直接参照)
            // ========================================================================
            const AnimState* currentState = nullptr;
            for (const auto& state : fsm.internalGraph.states) {
                if (state.stateHash == fsm.currentStateHash) {
                    currentState = &state;
                    break;
                }
            }

            if (!currentState) {
                fsm.currentStateHash = fsm.internalGraph.entryStateHash;
                return;
            }

            // ========================================================================
            // 3. 遷移評価を共通化するラムダ関数 (コードの重複を排除)
            // ========================================================================
            auto evaluateAndApplyTransition = [&](const AnimTransition& transition) -> bool {
                bool isTransitionReady = transition.conditions.empty() ? true :
                    (transition.logicType == AnimTransitionLogic::AND ? true : false);

                for (const auto& cond : transition.conditions) {
                    bool conditionPassed = false;
                    if (cond.op == AnimConditionOp::Trigger) {
                        conditionPassed = params.HasTrigger(cond.paramHash);
                    }
                    else if (cond.op == AnimConditionOp::Finished) {
                        conditionPassed = animator.isFinished;
                    }
                    else {
                        float paramValue = 0.0f;
                        bool isFound = false;
                        for (const auto& p : params.floats) {
                            if (p.hash == cond.paramHash) { paramValue = p.value; isFound = true; break; }
                        }
                        if (!isFound) {
                            for (const auto& p : params.bools) {
                                if (p.hash == cond.paramHash) { paramValue = p.value ? 1.0f : 0.0f; isFound = true; break; }
                            }
                        }
                        switch (cond.op) {
                        case AnimConditionOp::Greater: conditionPassed = (paramValue > cond.threshold); break;
                        case AnimConditionOp::Less:    conditionPassed = (paramValue < cond.threshold); break;
                        case AnimConditionOp::Equal:   conditionPassed = (std::abs(paramValue - cond.threshold) < 0.0001f); break;
                        }
                    }

                    if (transition.logicType == AnimTransitionLogic::AND) {
                        if (!conditionPassed) { isTransitionReady = false; break; }
                    }
                    else {
                        if (conditionPassed) { isTransitionReady = true; break; }
                    }
                }

                if (isTransitionReady) {
                    // トリガー消費
                    for (const auto& cond : transition.conditions) {
                        if (cond.op == AnimConditionOp::Trigger && params.HasTrigger(cond.paramHash)) {
                            params.ConsumeTrigger(cond.paramHash);
                            CCL_LOG_INFO(LogCategory::Game, "[AnimController] Consumed Trigger: '%s' (0x%08X)",
                                cond.paramName.c_str(), cond.paramHash);
                        }
                    }

                    std::string oldStateName = currentState ? currentState->sequenceName : "Unknown";
                    fsm.currentStateHash = transition.targetStateHash;

                    for (const auto& nextState : fsm.internalGraph.states) {
                        if (nextState.stateHash == transition.targetStateHash) {
                            CCL_LOG_SUCCESS(LogCategory::Game, "[AnimController] Entity %d Transitioned: [%s] -> [%s]",
                                (int)id, oldStateName.c_str(), nextState.sequenceName.c_str());

                            if (nextState.sequence) {
                                const AnimationCurve* curvePtr = nullptr;
                                if (!nextState.speedCurveName.empty()) {
                                    curvePtr = resourceManager.LoadAnimCurve(nextState.speedCurveName.c_str());
                                }
                                animator.Play(nextState.sequence, transition.blendDuration, nextState.isLoop, nextState.playbackSpeed);
                            }
                            break;
                        }
                    }
                    return true;
                }
                return false;
                };

            // ========================================================================
            // 4. Any State を最優先で評価
            // ========================================================================
            bool transitionFired = false;
            for (const auto& transition : fsm.internalGraph.anyStateTransitions) {
                // （オプショナル）すでに目標ステートにいる場合は無視する安全装置
                if (transition.targetStateHash == fsm.currentStateHash) continue;

                if (evaluateAndApplyTransition(transition)) {
                    transitionFired = true;
                    break;
                }
            }

            // 5. Any Stateからの割り込みが無ければ、通常遷移を評価
            if (!transitionFired) {
                for (const auto& transition : currentState->transitions) {
                    if (evaluateAndApplyTransition(transition)) {
                        break;
                    }
                }
            }
        });
}

// ========================================================================
// ★追加: アニメーション状態の3Dテキストオーバーレイ表示 (BossAISystem準拠)
// ========================================================================
void AnimControllerSystem::OnGui()
{
    if (!m_showFloatingText) return;

    // カメラ情報の取得
    if (!_world->HasResource<Camera>()) return;
    const auto* camera = _world->GetResource<Camera*>();
    if (!camera) return;

    // ViewProjection行列の計算
    Matrix viewProj = camera->GetView() * camera->GetProjection();


    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    float halfW = screenSize.x * 0.5f;
    float halfH = screenSize.y * 0.5f;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();

    ForEachWithID([&](CCL::ECS::EntityID id,
        AnimStateMachineComponent& fsm,
        AnimatorComponent& animator,
        AnimParametersComponent& params,
        const TransformComponent& trans)
        {
            // --- 1. 3D座標から2Dスクリーン座標への変換 ---
            DirectX::SimpleMath::Vector3 worldPos = trans.position + DirectX::SimpleMath::Vector3(0.0f, 2.0f, 0.0f);

            DirectX::SimpleMath::Vector4 clipPos = DirectX::SimpleMath::Vector4::Transform(
                DirectX::SimpleMath::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), viewProj);

            if (clipPos.w < 0.1f) return;

            DirectX::SimpleMath::Vector3 ndcPos = DirectX::SimpleMath::Vector3(
                clipPos.x / clipPos.w, clipPos.y / clipPos.w, clipPos.z / clipPos.w);

            if (ndcPos.x < -1.0f || ndcPos.x > 1.0f || ndcPos.y < -1.0f || ndcPos.y > 1.0f) return;

            float screenX = (ndcPos.x + 1.0f) * halfW;
            float screenY = (1.0f - ndcPos.y) * halfH;

            // --- 2. BossAISystem準拠の遠近法スケール計算 ---
            float dist = clipPos.w;
            float scale = m_useDepthScaling ? std::clamp(10.0f / dist, 0.3f, 1.5f) : 1.0f;
            float currentFontSize = m_debugFontSize * scale;

            // --- 3. 表示するテキスト情報の構築 ---
            std::string stateName = "Unknown";
            for (const auto& s : fsm.internalGraph.states) {
                if (s.stateHash == fsm.currentStateHash) {
                    stateName = s.sequenceName;
                    break;
                }
            }

            // Line 1: 現在のステート（または遷移状況）
            std::string line1 = "State: " + stateName;
            if (animator.isBlending && animator.previousSequence) {
                int blendPct = (int)((animator.currentBlendTime / animator.blendDuration) * 100.0f);
                line1 = "Blend: [" + animator.previousSequence->sequenceName + "] -> [" + stateName + "] " + std::to_string(blendPct) + "%";
            }

            // Line 2: 再生時間と速度
            float duration = animator.currentSequence ? animator.currentSequence->duration : 0.0f;
            char line2Buf[256];
            snprintf(line2Buf, sizeof(line2Buf), "Time: %.2f / %.2f (%.2fx)",
                animator.currentTimer, duration, animator.playbackSpeed);
            std::string line2 = line2Buf;

            ImVec2 textSize1 = font->CalcTextSizeA(currentFontSize, FLT_MAX, 0.0f, line1.c_str());
            ImVec2 textSize2 = font->CalcTextSizeA(currentFontSize, FLT_MAX, 0.0f, line2.c_str());

            float maxWidth = (std::max)(textSize1.x, textSize2.x);
            float totalHeight = textSize1.y + textSize2.y + 4.0f;

            float drawX1 = screenX - textSize1.x * 0.5f;
            float drawY1 = screenY - totalHeight * 0.5f;
            float drawX2 = screenX - textSize2.x * 0.5f;
            float drawY2 = drawY1 + textSize1.y + 4.0f;

            // --- 4. BossAISystem準拠の背景座布団描画 ---
            if (m_showTextBackground) {
                drawList->AddRectFilled(
                    ImVec2(screenX - maxWidth * 0.5f - 8.0f, drawY1 - 8.0f),
                    ImVec2(screenX + maxWidth * 0.5f + 8.0f, drawY2 + textSize2.y + 8.0f),
                    IM_COL32(0, 0, 0, 160), 8.0f);
            }

            // --- 5. BossAISystem準拠の8方向アウトライン描画 ---
            auto DrawOutlinedText = [&](float x, float y, ImU32 col, const char* text) {
                ImU32 outlineCol = IM_COL32(0, 0, 0, 255);
                drawList->AddText(font, currentFontSize, ImVec2(x - 1, y), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x + 1, y), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x, y - 1), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x, y + 1), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x - 1, y - 1), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x + 1, y - 1), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x - 1, y + 1), outlineCol, text);
                drawList->AddText(font, currentFontSize, ImVec2(x + 1, y + 1), outlineCol, text);
                // メイン文字
                drawList->AddText(font, currentFontSize, ImVec2(x, y), col, text);
                };

            ImU32 textCol1 = animator.isBlending ? IM_COL32(255, 200, 50, 255) : IM_COL32(100, 255, 100, 255);
            ImU32 textCol2 = IM_COL32(200, 200, 200, 255);

            DrawOutlinedText(drawX1, drawY1, textCol1, line1.c_str());
            DrawOutlinedText(drawX2, drawY2, textCol2, line2.c_str());
        });
}

REGISTER_LOGIC_SYSTEM(AnimControllerSystem, Priority::LogicStage::L02_PostUpdate);