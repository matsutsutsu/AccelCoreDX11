#pragma once
#include "IAudioAPI.h"
#include <fmod_studio.hpp>
#include <vector>

/**
 * @brief FMOD Studioを用いた IAudioAPI の実装クラス。
 * @note std::unordered_mapを排除し、std::vectorによる連続メモリレイアウト(DOD)を徹底。
 * キャッシュライン効率を最大化し、Swap & Popによる高速なインスタンス管理を実現している。
 */
class FmodAudioManager : public IAudioAPI {
private:
    FMOD::Studio::System* m_studioSystem = nullptr;

    /// @brief ロードされたBankデータを保持し、破棄時に一括解放するためのリスト
    std::vector<FMOD::Studio::Bank*> m_loadedBanks;

    /// @brief イベントパスのハッシュ値とFMODのEventDescriptionのペア。ソートして二分探索に使用する。
    std::vector<std::pair<uint32_t, FMOD::Studio::EventDescription*>> m_eventCache;

    /**
     * @brief 継続再生中のインスタンス情報。DOD準拠のPOD(Plain Old Data)として扱う。
     */
    struct PlayingInstance {
        uint32_t id;
        FMOD::Studio::EventInstance* instance;
    };

    /// @brief 継続再生(PlayEvent) の管理用フラット配列
    std::vector<PlayingInstance> m_playingInstances;
    uint32_t m_nextPlayingId = 1;

    /**
     * @brief DirectXのベクトルをFMOD専用のベクトル構造体に変換する
     */
    inline FMOD_VECTOR ToFMOD(const DirectX::XMFLOAT3& v) const { return { v.x, v.y, v.z }; }

    /**
     * @brief FMOD関数の戻り値を検査し、エラーがあればログを出力する。
     */
    bool CheckFMODError(FMOD_RESULT result, const char* operationName) const;

    /**
     * @brief ソート済みキャッシュから二分探索(O(log N))でイベントを取得する。
     */
    FMOD::Studio::EventDescription* FindEvent(uint32_t eventHash) const;

    /**
     * @brief 継続再生中のインスタンスを線形探索(O(N))で取得する。
     * @note インスタンス数が数千に満たない場合、連続メモリの線形探索はツリー走査よりキャッシュヒット率が高く高速に動作する。
     */
    FMOD::Studio::EventInstance* FindInstance(uint32_t playingId) const;

public:
    FmodAudioManager() = default;
    virtual ~FmodAudioManager();

    // =======================================================
    // 💥 [修正点] 名前の隠蔽(Name Hiding)を回避する
    // =======================================================
    // FmodAudioManagerで uint32_t 版をオーバーライドしたことにより、
    // ベースクラスの const char* 版が隠蔽されてしまうのを防ぐ。
    using IAudioAPI::PlayOneShot;
    using IAudioAPI::PlayOneShot3D;
    using IAudioAPI::PlayEvent;
    using IAudioAPI::PlayEvent3D;

    /**
     * @brief FMODシステムを生成し、カスタムメモリアロケータをフックして初期化する。
     */
    void Initialize() override;

    /**
     * @brief 3D音響の計算や、再生終了したインスタンスのメモリ解放を行う。
     * @note DODに基づき、配列の中間削除は行わず、Swap & PopによるO(1)のクリーンアップを実行する。
     */
    void Update() override;

    /**
     * @brief 指定されたパスのBankファイルをメモリにロードする。
     * @param bankFilePath 例: "Assets/Audio/Master.bank"
     * @warning Strings Bank ("*.strings.bank") は他のどのBankよりも先にロードしなければならない。
     */
    void LoadBank(const char* bankFilePath) override;

    /**
     * @brief FMODのイベントパスから設計図を取得し、一時キャッシュに登録する。
     * @param eventPath 例: "event:/SFX/Shoot"
     */
    void LoadEvent(const char* eventPath) override;

    /**
     * @brief JSON設定ファイルからAudioのBankとEventを一括ロードする。
     * @param configFilePath 例: "Assets/Config/AudioConfig.json"
     */
    void LoadConfig(const std::string& configFilePath) override;

    /**
     * @brief すべてのイベントのロードが完了した後、キャッシュをハッシュ値で昇順ソートする。
     */
    void FinalizeEventLoading() override;

    /**
     * @brief 対象の音声を2Dワンショット（Fire & Forget）で再生する。
     * @param eventHash 対象イベントのハッシュ値
     */
    void PlayOneShot(uint32_t eventHash) override;

    /**
     * @brief 指定した3D座標でワンショット音声を再生する。
     * @param eventHash 対象イベントのハッシュ値
     * @param position ワールド座標
     */
    void PlayOneShot3D(uint32_t eventHash, const DirectX::XMFLOAT3& position) override;

    /**
     * @brief 継続再生用のインスタンスを生成・再生し、制御用IDを発行する。
     * @param eventHash 対象イベントのハッシュ値
     * @return uint32_t 制御用ID (playingId)。失敗時は0。
     */
    uint32_t PlayEvent(uint32_t eventHash) override;

    /**
     * @brief 3D空間の指定座標で継続再生用のインスタンスを生成・再生し、制御用IDを発行する。
     * @param eventHash 対象イベントのハッシュ値
     * @param position 初期ワールド座標
     * @return uint32_t 制御用ID (playingId)。失敗時は0。
	 */ 
    uint32_t PlayEvent3D(uint32_t eventHash, const DirectX::XMFLOAT3& position) override; // ★追加

    /**
     * @brief 指定されたIDの音声再生を停止する。
     * @param playingId PlayEvent() が返したID
     * @param allowFadeOut trueの場合、FMOD側のエンベロープに従ってフェードアウトする。
     */
    void StopEvent(uint32_t playingId, bool allowFadeOut = true) override;

    /**
     * @brief 再生中の音声に対して、動的パラメータを送信する。
     * @param playingId PlayEvent() が返したID
     * @param paramName パラメータ名 (例: "RPM")
     * @param value 変更値
     */
    void SetEventParameter(uint32_t playingId, const char* paramName, float value) override;

     /**
     * @brief 継続再生中のイベントの3D座標と向きを更新する。
     * @param playingId PlayEvent() が返したID
     * @param pos 最新のワールド座標
     */
    void SetEvent3DAttributes(uint32_t playingId, const DirectX::XMFLOAT3& pos) override;

    /**
     * @brief プレイヤーの座標と向きをオーディオエンジンに設定する。
     */
    void SetListenerPosition(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& up) override;
};