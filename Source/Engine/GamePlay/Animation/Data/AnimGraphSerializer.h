#pragma once
#include "AnimStateMachine.h"
#include <string>

// 便宜上、AnimSequenceを検索するためのダミーリソース管理の仕組みを想定しています。
// 実際のプロジェクトの ResourceManager 等に合わせて調整してください。
class AnimGraphSerializer {
public:
    // グラフをJSONファイルとして保存する
    static bool SaveToJSON(const AnimStateGraph& graph, const std::string& filepath);

    // JSONファイルからグラフを読み込む
    // 引数 availableSequences: ポインタを紐付けるための実体リスト（あるいはResourceManager）
    static bool LoadFromJSON(AnimStateGraph& outGraph, const std::string& filepath, const std::vector<AnimSequence>& availableSequences);
};