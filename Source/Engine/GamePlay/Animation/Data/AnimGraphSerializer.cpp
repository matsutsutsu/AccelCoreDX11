#include "AnimGraphSerializer.h"
#include "Engine/Platform/Logger.h"
#include <fstream>
#include <iomanip> // std::setw 用

bool AnimGraphSerializer::SaveToJSON(const AnimStateGraph& graph, const std::string& filepath)
{
    std::ofstream os(filepath);
    if (!os.is_open()) return false;

    try {
        json j = graph; // to_json が自動的に呼ばれる
        os << std::setw(4) << j << std::endl; // 整形して出力
    }
    catch (const std::exception& e) {
        CCL_LOG_ERROR(LogCategory::Game, "JSON Save Error: %s", e.what());
        return false;
    }
    return true;
}

bool AnimGraphSerializer::LoadFromJSON(AnimStateGraph& outGraph, const std::string& filepath, const std::vector<AnimSequence>& availableSequences)
{
    std::ifstream is(filepath);
    if (!is.is_open()) return false;

    try {
        json j;
        is >> j;
        outGraph = j.get<AnimStateGraph>(); // from_json が自動的に呼ばれる

        // ★ ポインタの紐付け (Binding)
        for (auto& state : outGraph.states) {
            state.sequence = nullptr;
            for (const auto& seq : availableSequences) {
                if (seq.sequenceName == state.sequenceName) {
                    state.sequence = &seq;
                    break;
                }
            }
        }
    }
    catch (const std::exception& e) {
        CCL_LOG_ERROR(LogCategory::Game, "JSON Load Error: %s", e.what());
        return false;
    }
    return true;
}