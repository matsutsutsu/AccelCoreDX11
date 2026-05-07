#pragma once
#include <string>
#include <DirectXMath.h>
#include "Engine/Core/Math/StringHash.h" // ※プロジェクトのパスに合わせてください

/**
 * @brief ゲーム内のすべての音声再生を管理するオーディオAPIのインターフェース。
 * * 物理エンジンや描画エンジンから完全に独立し、ECSシステムに対して
 * イベント駆動ベースの音声再生機能を提供する。
 * 内部でFMODなどのミドルウェアを使用することを想定。
 */
class IAudioAPI {
public:
    virtual ~IAudioAPI() = default;

    // =======================================================
    // システム管理ライフサイクル
    // =======================================================

    /**
     * @brief オーディオシステムの初期化。Scene遷移時などに1度だけ呼ばれるべき。
     */
    virtual void Initialize() = 0;

    /**
     * @brief 毎フレームのオーディオ処理の更新。
     * 3D音響の計算更新や、再生が終了したメモリの自動お掃除を行う。
     */
    virtual void Update() = 0;

    // =======================================================
    // データロード
    // =======================================================

    /**
     * @brief JSON設定ファイルからAudioのBankとEventを一括ロードする。
     * @param configFilePath 例: "Assets/Config/AudioConfig.json"
     */
    virtual void LoadConfig(const std::string& configFilePath) = 0;

    /**
     * @brief FMODのBankファイル（音声波形の塊）をメモリにロードする。
     * @param bankFilePath 例: "Assets/Audio/Master.bank"
     */
    virtual void LoadBank(const char* bankFilePath) = 0;

    /**
     * @brief イベントの設計図を辞書に登録する。
     * @param eventPath 例: "event:/SFX/Shoot"
     */
    virtual void LoadEvent(const char* eventPath) = 0;

    /**
     * @brief ロードされたイベント辞書のソートを確定させる。
     * @note データ駆動での一括ロード完了後に必ず1回呼び出すこと。二分探索によるO(log N)検索を可能にするため。
     */
    virtual void FinalizeEventLoading() = 0;

    // =======================================================
    // 💥 ワンショット再生 (Fire & Forget)
    // =======================================================
    // 銃声、足音、UIのクリック音など「鳴らしたら後はシステム任せで良い音」用。
    // 再生後の停止制御やパラメータ変更はできません。

    /**
     * @brief ワンショット音声を2D（画面全体）で再生する。
     * @param eventHash 対象イベントのハッシュ値
     */
    virtual void PlayOneShot(uint32_t eventHash) = 0;

    /**
     * @brief ワンショット音声を3D空間の指定座標で再生する。
     * @param eventHash 対象イベントのハッシュ値
     * @param position ワールド座標
     */
    virtual void PlayOneShot3D(uint32_t eventHash, const DirectX::XMFLOAT3& position) = 0;

    // =======================================================
    // 🎵 継続再生と制御 (BGM / 環境音)
    // =======================================================
    // BGMや車のエンジン音など「後から止めたい」「ピッチを変えたい」音用。
    // 必ず戻り値のID（Handle）を保持し、それを鍵として制御します。

    /**
     * @brief 音声を再生し、それを後から制御するためのIDを発行する。
     * @param eventHash 対象イベントのハッシュ値
     * @return uint32_t 再生中インスタンスを識別するユニークなID。失敗時は0を返す。
     */
    virtual uint32_t PlayEvent(uint32_t eventHash) = 0;

    /**
     * @brief 3D空間の指定座標で継続再生用のインスタンスを生成・再生し、制御用IDを発行する。
     * @param eventHash 対象イベントのハッシュ値
     * @param position 初期ワールド座標
     * @return uint32_t 制御用ID (playingId)。失敗時は0。
     */
    virtual uint32_t PlayEvent3D(uint32_t eventHash, const DirectX::XMFLOAT3& position) = 0;

    /**
     * @brief 指定したIDの音声を停止する。
     * @param playingId PlayEvent() で受け取ったID
     * @param allowFadeOut trueならFMOD Studioで設定したフェードアウト時間を尊重して止まる。
     */
    virtual void StopEvent(uint32_t playingId, bool allowFadeOut = true) = 0;

    /**
     * @brief 指定したIDの音声に対して、FMOD内で設定した動的パラメータを送る。
     * @param playingId PlayEvent() で受け取ったID
     * @param paramName パラメータ名 (例: "EnemyDistance", "RPM")
     * @param value 変更する数値
     */
    virtual void SetEventParameter(uint32_t playingId, const char* paramName, float value) = 0;

    /**
     * @brief 継続再生中のイベントの3D座標と向きを更新する。
     * @param playingId PlayEvent() が返したID
     * @param pos 最新のワールド座標
     */
    virtual void SetEvent3DAttributes(uint32_t playingId, const DirectX::XMFLOAT3& pos) = 0;

    // =======================================================
    // 3Dリスナー設定
    // =======================================================

    /**
     * @brief プレイヤー（耳）の位置と向きをオーディオエンジンに伝える。
     * これにより、音源からの距離による減衰や、左右のパンニングが計算される。
     */
    virtual void SetListenerPosition(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& up) = 0;

    // =======================================================
    // 既存コードやスクリプトから呼びやすくするためのインラインヘルパー
    // =======================================================
    void PlayOneShot(const char* eventPath) { PlayOneShot(CCL::Utils::HashString(eventPath)); }
    void PlayOneShot3D(const char* eventPath, const DirectX::XMFLOAT3& position) { PlayOneShot3D(CCL::Utils::HashString(eventPath), position); }
    uint32_t PlayEvent(const char* eventPath) { return PlayEvent(CCL::Utils::HashString(eventPath)); }
};