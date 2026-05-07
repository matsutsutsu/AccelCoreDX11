#pragma once

// タグ
#include "Game/Logics/Combat/DeadTag.h"

// 追加したコンポーネントをここで記述するオールヘッダー
#include "Engine/GamePlay/Transform/TransformComponent.h"

#include "Engine/GamePlay/Core/NameComponent.h"

// 物理
#include "Engine/GamePlay/Physics/RigidBody/JoltRigidbodyComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltBoxColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltSphereColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltCapsuleColliderComponent.h"

// キャラクターコントローラー
#include "Engine/GamePlay/Physics/Character/JoltCharacterHandleComponent.h"
#include "Engine/GamePlay/Physics/Character/JoltCharacterConfigComponent.h"


#include "Game/Logics/Character/Enemy/EnemyComponent.h"

// カメラ

#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

// 描画
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/GamePlay/Graphics/Core/PrimitiveComponent.h"
#include "Engine/GamePlay/Graphics/Core/MaterialComponent.h"

//#include "Game/GameLogics/Text/FloatingTextComponent.h"

#include "Engine/GamePlay/Animation/AnimatorComponent.h"
// シェーダー
#include "Engine/GamePlay/Graphics/Lighting/FogComponent.h"
#include "Engine/GamePlay/Graphics/Lighting/LightComponent.h"
#include "Engine/GamePlay/Graphics/Lighting/ShadowMapConfigComponent.h"
#include "Engine/GamePlay/Graphics/PostProcess/BloomConfigComponent.h"
#include "Engine/GamePlay/Graphics/PostProcess/ToneMapConfigComponent.h"

#include "Engine/GamePlay/Graphics/Particle/GPUParticleComponent.h"
#include "Game/Utils/PooledParticleComponent.h"

// ゲームロジック


#include "Game/Logics/Character/Player/PlayerComponent.h"
#include "Game/Logics/Character/PlayerCar/PlayerCarComponent.h"


#include "Game/Logics/Combat/HealthComponent.h"
#include "Engine/GamePlay/Utils/TimerComponent.h"


#include "Engine/Scripting/LuaScriptComponent.h"

#include "Engine/GamePlay/Transform/PendingParentComponent.h"

