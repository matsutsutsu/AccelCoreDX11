#pragma once

// 描画用モデル(ModelComponent)の頂点データから、
// 自動的に地形用のMeshShapeを生成させるための「目印（タグ）」コンポーネント。
// ★ 注意: このコンポーネントと同時に ModelComponent をアタッチしておくこと。
struct JoltMeshColliderComponent {
    bool isEnabled = true; // インスペクタ表示用のダミー変数
};