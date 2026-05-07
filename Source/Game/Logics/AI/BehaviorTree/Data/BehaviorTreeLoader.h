/**
 * @file BehaviorTreeLoader.h
 * @brief エディタが出力したJSONからランタイム用のBTAssetを構築する
 */
#pragma once
#include "BehaviorTreeData.h"
#include <string>


class BehaviorTreeLoader {
public:
    /**
     * @brief JSONファイルからBTAssetを読み込む
     */
    static bool LoadFromJson(const std::string& path, BTAsset& outAsset);

private:
    static BTNodeType StringToNodeType(const std::string& typeStr);
};
