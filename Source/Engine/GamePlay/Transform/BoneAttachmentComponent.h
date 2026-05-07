#include <string>

/**
 * @brief 親エンティティの特定ボーンに追従させるためのコンポーネント
 */
struct BoneAttachmentComponent {
    // エディタ設定用（"mixamorig:RightHand" など）
    std::string boneName = "";

    // 毎フレーム文字列検索するのは最悪の設計なので、
    // 生成時（または初期化時）にボーンのインデックスをキャッシュする
    int cachedBoneIndex = -1;
};