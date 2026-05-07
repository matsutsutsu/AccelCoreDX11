#pragma once
#include <DirectXCollision.h>

// AABB（境界箱）のデータのみを保持するコンポーネント
struct BoundingBoxComponent {
    DirectX::BoundingBox worldAABB;
};

// 描画されるかどうかのフラグのみを保持するコンポーネント
struct VisibilityComponent {
    bool isVisible = true;
};