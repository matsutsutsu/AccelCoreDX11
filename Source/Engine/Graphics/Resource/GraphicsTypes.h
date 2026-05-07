#pragma once


// マテリアルの透明度のモード
enum class AlphaMode
{
	Opaque, // 不透明
	Mask,   // マスク透過（アルファカットオフ）
	Blend   // アルファブレンド
};