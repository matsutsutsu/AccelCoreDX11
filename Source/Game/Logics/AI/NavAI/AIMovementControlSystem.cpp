#include "AIMovementControlSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;

// ===================================================================================
// 【 AI意思決定 (速度制御) : AIMovementControlSystem 】
//
// [ 役割 ]
// AIの「今の気分(State)」を、ナビゲーションシステムが理解できる「具体的な移動速度(Speed)」に翻訳する橋渡し役。
//
// [ ECSデータパイプライン ]
// 📥 READ  : AIStateComponent        (現在のステート: Patrol, Chase, Investigate 等)
// 📤 WRITE : NavAgentComponent       (currentDesiredSpeed: ナビゲーションシステムに対する希望速度)
//
// [ 内部挙動の直感的な解説 ]
// ゲーム固有の「AIの感情」と、エンジン汎用の「ナビゲーション」を疎結合（分離）するための極めて重要なシステムです。
// 物理的な計算や経路探索は一切行わず、「追跡中だから時速5.0mで走れ」「巡回中だから時速1.5mで歩け」という
// 速度パラメータの代入のみを行います。このシステムのおかげで、汎用システム側はAIの都合を知る必要がなくなります。
// ===================================================================================

void AIMovementControlSystem::Update(float dt) {
    ForEach([&](const AIStateComponent& state, NavAgentComponent& navAgent) {

        // ピエロのステートに応じて、NavAgentに「出したい速度」を命令する
        switch (state.currentState) {
        case AIState::Patrol:
            navAgent.currentDesiredSpeed = state.patrolSpeed;
            break;
        case AIState::Investigate:
            navAgent.currentDesiredSpeed = state.investigateSpeed; 
            break;
        case AIState::Chase:
            navAgent.currentDesiredSpeed = state.chaseSpeed;      
            break;
        case AIState::AttackDoor:
        case AIState::Idle:
        default:
            navAgent.currentDesiredSpeed = 0.0f; // 停止
            break;
        }
        });
}


REGISTER_LOGIC_SYSTEM(AIMovementControlSystem, Priority::LogicStage::L02_AI_Act)