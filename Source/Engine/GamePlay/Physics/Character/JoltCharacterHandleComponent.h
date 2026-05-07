#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

// 仮想キャラクターの「実体」を保持するシステム用コンポーネント
struct JoltCharacterHandleComponent {
    // Joltのスマートポインタ。エンティティ破棄時に自動でメモリが解放されます。
    JPH::Ref<JPH::CharacterVirtual> character;
};