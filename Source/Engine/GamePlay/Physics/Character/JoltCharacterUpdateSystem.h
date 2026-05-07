#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "JoltCharacterHandleComponent.h"
#include "Game/Logic/Character/CharacterMovementInputComponent.h"
#include "Engine/GamePlay/Physics/Character/JoltCharacterConfigComponent.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

// ===================================================================================
// ファイル名: JoltCharacterUpdateSystem.h
// 役割:
// 仮想キャラクター（CharacterVirtual）の移動・壁ずり・段差登りを計算する専用システム。
//
// 【アーキテクチャ仕様】
// - 実行フェーズ: PrePhysics (★ JoltStepSystem
// が走る前に終わらせるのが公式の絶対ルール)
// - 対象: JoltCharacterHandleComponent を持つすべてのEntity。
//
// 【使い方・ルール】
// -
// プレイヤーや敵の「意思（SetLinearVelocity）」を受け取り、地形との複雑な干渉を並列計算する。
// - 計算結果の「位置(Position)」だけを Transform
// に書き戻す。回転(Rotation)は絶対に同期しないこと。
// - isTeleported フラグが立っている場合は、計算前にワープ処理を優先して行う。
// ===================================================================================

class JoltCharacterUpdateSystem
    : public CCL::ECS::IfSystem<JoltCharacterUpdateSystem,
                                CCL::ECS::Write<TransformComponent>,
                                CCL::ECS::Read<JoltCharacterHandleComponent>,
                                CCL::ECS::Read<CharacterMovementInputComponent>,
                                CCL::ECS::Read<JoltCharacterConfigComponent>,
                                CCL::ECS::Read<TimeState >> // ★追加
{
    public:
  JoltCharacterUpdateSystem() : IfSystem("JoltCharacterUpdateSystem") {}
  virtual ~JoltCharacterUpdateSystem() = default;

  void Update(float dt) override;
};