#pragma once
#include "Engine/Physics/JoltPhysicsManager.h" // PhysicsLayers用
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/MotionType.h>

// ===================================================================================
// ファイル名: JoltRigidbodyComponent.h
// 役割: 物理挙動の「設計図（パラメータ）」を定義するデータコンポーネント。
//
// 【アーキテクチャ仕様】
// - 純粋なデータ(DOD)であり、Joltのポインタ等は一切持たない。
// - エディタ(ImGui)やJSONからシリアライズされる値。
//
// 【使い方・ルール】
// - これと
// TransformComponent、および形状(Box/Sphere等)の3つをEntityにアタッチすると、
//   JoltBodySetupSystem が自動的にJolt空間に剛体（実体）を生成する。
// -
// 初期化後は、このコンポーネントの値を変更してもJoltには反映されない（初期パラメータとして扱う）。
// ===================================================================================


// ユーザーがエディタやJSONで付与し、物理の性質を設定するためのコンポーネント
struct JoltRigidbodyComponent {
    JPH::EMotionType motionType  = JPH::EMotionType::Dynamic;
    JPH::ObjectLayer objectLayer = PhysicsLayers::MOVING;
    float            restitution = 0.0f; // 反発係数
    float            friction    = 0.2f; // 摩擦係数

    // =========================================================
    //  弾丸や特殊な物理挙動のための拡張パラメータ
    // =========================================================
    DirectX::XMFLOAT3 initialVelocity = {0.0f, 0.0f, 0.0f}; // 生成された瞬間の初速
    float             gravityFactor   = 1.0f;               // 重力の効き具合（0.0fで無重力ビームになる）
    bool              isSensor        = false;              // trueなら物理的にぶつからず、すり抜ける（ヒット判定のみ取る）
    bool              useCCD          = false;              // trueなら連続衝突判定(高速な弾の壁抜け防止)を有効化
};