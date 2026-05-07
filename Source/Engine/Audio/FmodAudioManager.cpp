#include "FmodAudioManager.h"
#include "Engine/Platform/Logger.h"
#include <algorithm>
#include <fstream>
#include <json.hpp> // ※プロジェクトのパスに合わせてください (例: <nlohmann/json.hpp>)

// ===========================================================
// 内部ユーティリティ
// ===========================================================
bool FmodAudioManager::CheckFMODError(FMOD_RESULT result, const char* operationName) const {
    if (result != FMOD_OK) {
        CCL_LOG_ERROR(LogCategory::Core, "[FMOD] %s failed! Error Code: %d", operationName, result);
        return false;
    }
    return true;
}

FMOD::Studio::EventDescription* FmodAudioManager::FindEvent(uint32_t eventHash) const {
    // ソート済みvectorに対する二分探索 (O(log N))。ハッシュの衝突がない前提。
    auto it = std::lower_bound(m_eventCache.begin(), m_eventCache.end(), eventHash,
        [](const std::pair<uint32_t, FMOD::Studio::EventDescription*>& element, uint32_t hash) {
            return element.first < hash;
        });

    if (it != m_eventCache.end() && it->first == eventHash) {
        return it->second;
    }
    return nullptr;
}

FMOD::Studio::EventInstance* FmodAudioManager::FindInstance(uint32_t playingId) const {
    // 連続メモリの線形探索。プリフェッチによりキャッシュミスが極小に抑えられる。
    for (const auto& inst : m_playingInstances) {
        if (inst.id == playingId) return inst.instance;
    }
    return nullptr;
}

FmodAudioManager::~FmodAudioManager() {
    if (m_studioSystem) {
        // 継続再生中のインスタンスを強制停止して解放
        for (auto& inst : m_playingInstances) {
            inst.instance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            inst.instance->release();
        }
        m_playingInstances.clear();

        m_studioSystem->unloadAll();
        m_studioSystem->release();
        m_studioSystem = nullptr;
    }
}

// ===========================================================
// システムライフサイクル
// ===========================================================
void FmodAudioManager::Initialize() {
    // 💥 [修正点] メモリフックをFMODのデフォルトに委譲する
    // マクロ展開エラー（F_CALLBACK未定義等）を根本から回避するため、
    // 全て nullptr を渡し、FMOD内部の標準 malloc/free を使用させます。
    FMOD::Memory_Initialize(nullptr, 0, nullptr, nullptr, nullptr, FMOD_MEMORY_ALL);

    CheckFMODError(FMOD::Studio::System::create(&m_studioSystem), "System Create");
    CheckFMODError(m_studioSystem->initialize(1024, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr), "System Initialize");

    m_playingInstances.reserve(256);
    m_eventCache.reserve(512);
}

void FmodAudioManager::Update() {
    if (!m_studioSystem) return;

    // 【DOD最適化】Swap & Pop による高速なインスタンスクリーンアップ
    for (size_t i = 0; i < m_playingInstances.size(); ) {
        FMOD_STUDIO_PLAYBACK_STATE state;
        m_playingInstances[i].instance->getPlaybackState(&state);

        if (state == FMOD_STUDIO_PLAYBACK_STOPPED) {
            // FMODにメモリ破棄を許可
            m_playingInstances[i].instance->release();

            // 末尾の要素を現在の位置に上書きコピー (Swap)
            m_playingInstances[i] = m_playingInstances.back();
            // 配列のサイズを1縮小 (Pop)。これによりメモリの隙間（フラグメンテーション）が発生しない
            m_playingInstances.pop_back();

            // i をインクリメントしない（末尾から持ってきた要素を次ループで同じインデックスとして評価するため）
        }
        else {
            ++i;
        }
    }

    m_studioSystem->update();
}

// ===========================================================
// データロード
// ===========================================================

void FmodAudioManager::LoadConfig(const std::string& configFilePath) {
    std::ifstream audioConfigFile(configFilePath);
    if (audioConfigFile.is_open()) {
        nlohmann::json audioConfig;

        // -------------------------------------------------------------
        //  JSONのパース処理を try-catch で保護し、クラッシュを防ぐ
        // -------------------------------------------------------------
        try {
            audioConfigFile >> audioConfig;
        }
        catch (const nlohmann::json::parse_error& e) {
            // パースエラー発生時、エンジンの実行は止めず、何行目で間違えたかをログに出す
            CCL_LOG_ERROR(LogCategory::Core, "JSON Parse Error in %s : %s", configFilePath.c_str(), e.what());
            return;
        }

        // 【最重要】文字列の解決を行う Strings Bank は、他のBankより「絶対に先」にロードしなければならない。
        if (audioConfig.contains("StringsBank")) {
            LoadBank(audioConfig["StringsBank"].get<std::string>().c_str());
        }
        else {
            CCL_LOG_ERROR(LogCategory::Core, "AudioConfig.json is missing 'StringsBank'. FMOD string resolution will fail.");
        }

        // 通常のBankのロード
        if (audioConfig.contains("Banks")) {
            for (const auto& bankPath : audioConfig["Banks"]) {
                LoadBank(bankPath.get<std::string>().c_str());
            }
        }

        // Event(設計図)のロード
        if (audioConfig.contains("Events")) {
            for (const auto& eventPath : audioConfig["Events"]) {
                LoadEvent(eventPath.get<std::string>().c_str());
            }
        }

        // ロード完了後、ハッシュルックアップ用にキャッシュをソートする
        FinalizeEventLoading();

        CCL_LOG_SUCCESS(LogCategory::Core, "Audio configuration loaded via JSON successfully from %s", configFilePath.c_str());
    }
    else {
        CCL_LOG_WARN(LogCategory::Core, "Failed to open AudioConfig: %s", configFilePath.c_str());
    }
}

void FmodAudioManager::LoadBank(const char* bankFilePath) {
    FMOD::Studio::Bank* bank = nullptr;
    if (CheckFMODError(m_studioSystem->loadBankFile(bankFilePath, FMOD_STUDIO_LOAD_BANK_NORMAL, &bank), "Load Bank")) {
        m_loadedBanks.push_back(bank);
    }
}

void FmodAudioManager::LoadEvent(const char* eventPath) {
    FMOD::Studio::EventDescription* eventDesc = nullptr;
    if (CheckFMODError(m_studioSystem->getEvent(eventPath, &eventDesc), "Get Event Description")) {
        uint32_t hash = CCL::Utils::HashString(eventPath);
        m_eventCache.push_back({ hash, eventDesc });
    }
}

void FmodAudioManager::FinalizeEventLoading() {
    // 二分探索のために、ハッシュ値の昇順で配列をソートする
    std::sort(m_eventCache.begin(), m_eventCache.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
        });
}

// ===========================================================
// ワンショット再生 (SE)
// ===========================================================
void FmodAudioManager::PlayOneShot(uint32_t eventHash) {
    if (auto eventDesc = FindEvent(eventHash)) {
        FMOD::Studio::EventInstance* inst = nullptr;
        if (CheckFMODError(eventDesc->createInstance(&inst), "Create OneShot Instance")) {
            inst->start();
            // 自動解放の予約。Fire & Forget を実現する。
            inst->release();
        }
    }
}

void FmodAudioManager::PlayOneShot3D(uint32_t eventHash, const DirectX::XMFLOAT3& position) {
    if (auto eventDesc = FindEvent(eventHash)) {
        FMOD::Studio::EventInstance* inst = nullptr;
        if (CheckFMODError(eventDesc->createInstance(&inst), "Create OneShot3D Instance")) {
            FMOD_3D_ATTRIBUTES attributes = { {0} };
            attributes.position = ToFMOD(position);
            attributes.forward = { 0, 0, 1 };
            attributes.up = { 0, 1, 0 };
            inst->set3DAttributes(&attributes);

            inst->start();
            inst->release();
        }
    }
}

// ===========================================================
// 継続再生と制御 (BGM / 環境音)
// ===========================================================
uint32_t FmodAudioManager::PlayEvent(uint32_t eventHash) {
    if (auto eventDesc = FindEvent(eventHash)) {
        FMOD::Studio::EventInstance* inst = nullptr;
        if (CheckFMODError(eventDesc->createInstance(&inst), "Create Event Instance")) {
            inst->start();

            uint32_t playingId = m_nextPlayingId++;
            m_playingInstances.push_back({ playingId, inst });
            return playingId;
        }
    }
    return 0; // 失敗時
}

uint32_t FmodAudioManager::PlayEvent3D(uint32_t eventHash, const DirectX::XMFLOAT3& position) {
    if (auto eventDesc = FindEvent(eventHash)) {
        FMOD::Studio::EventInstance* inst = nullptr;
        if (CheckFMODError(eventDesc->createInstance(&inst), "Create Event3D Instance")) {
            FMOD_3D_ATTRIBUTES attributes = { {0} };
            attributes.position = ToFMOD(position);
            attributes.forward = { 0, 0, 1 };
            attributes.up = { 0, 1, 0 };
            inst->set3DAttributes(&attributes);

            inst->start();

            uint32_t playingId = m_nextPlayingId++;
            m_playingInstances.push_back({ playingId, inst });
            return playingId;
        }
    }
    return 0;
}

void FmodAudioManager::StopEvent(uint32_t playingId, bool allowFadeOut) {
    if (auto inst = FindInstance(playingId)) {
        FMOD_STUDIO_STOP_MODE mode = allowFadeOut ? FMOD_STUDIO_STOP_ALLOWFADEOUT : FMOD_STUDIO_STOP_IMMEDIATE;
        inst->stop(mode);
    }
}

void FmodAudioManager::SetEventParameter(uint32_t playingId, const char* paramName, float value) {
    if (auto inst = FindInstance(playingId)) {
        inst->setParameterByName(paramName, value);
    }
}


void FmodAudioManager::SetEvent3DAttributes(uint32_t playingId, const DirectX::XMFLOAT3& pos) {
    if (auto inst = FindInstance(playingId)) {
        FMOD_3D_ATTRIBUTES attributes = { {0} };
        attributes.position = ToFMOD(pos);
        attributes.forward = { 0, 0, 1 }; // 必要に応じてエンティティの向きも渡せるように拡張可能
        attributes.up = { 0, 1, 0 };
        inst->set3DAttributes(&attributes);
    }
}

void FmodAudioManager::SetListenerPosition(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& up) {
    if (!m_studioSystem) return;

    FMOD_3D_ATTRIBUTES attributes = { {0} };
    attributes.position = ToFMOD(pos);
    attributes.forward = ToFMOD(forward);
    attributes.up = ToFMOD(up);
    m_studioSystem->setListenerAttributes(0, &attributes, nullptr);
}