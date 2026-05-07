/**
 * @file BehaviorTreeWindow.h
 * @brief ビヘイビアツリーを視覚的に構築・編集するためのエディタウィンドウ
 * @note 【設計思想】
 * ランタイム（DOD向けフラット配列）とは完全に切り離された「エディタ専用のデータ構造」を持ちます。
 * 人間が編集しやすいグラフ構造（ノードとリンクの網目）を管理し、
 * 最終的にこれを機械が読みやすい配列（JSON）に変換（Bake）する役割を担います。
 */
#pragma once
#include "Editor/Core/EditorWindow.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeData.h"
#include <string>
#include <vector>
#include <json.hpp>

namespace ax { namespace NodeEditor { struct EditorContext; } }

/**
 * @enum EditorBTNodeType
 * @brief エディタ上で視覚的に区別されるノードの種類
 */
enum class EditorBTNodeType {
    Root,      ///< 始点（1グラフに1つのみ）。入力ピンを持たない。
    Selector,  ///< 複合ノード：子を順番に実行し、1つでも成功すれば成功。
    Sequence,  ///< 複合ノード：子を順番に実行し、1つでも失敗すれば失敗。
    Condition, ///< 葉ノード：Blackboardの値を評価し、真偽を返す。出力ピンを持たない。
    Action,    ///< 葉ノード：実際の行動（攻撃、移動）を行う。出力ピンを持たない。
    Group,     ///< コメント用グループノード（機能的な実行順序は持たない）
	Decorator  ///< デコレータノード（子ノードの挙動を修飾する。例: Inverter, Cooldown, Retryなど）
};

/**
 * @struct BTPin
 * @brief ノード同士を接続するための「接続口」のデータ
 */
struct BTPin {
    uint32_t id;       ///< ピンのユニークID（ImGui Node Editorで必須）
    uint32_t nodeId;   ///< このピンが属しているノードのID
    bool isInput;      ///< true なら左側の入力ピン、false なら右側の出力ピン
};

/**
 * @struct BTLink
 * @brief ピンとピンを繋ぐ「線（リンク）」のデータ
 */
struct BTLink {
    int id;               ///< リンクのユニークID
    uint32_t startPinId;  ///< 接続元の出力ピンID
    uint32_t endPinId;    ///< 接続先の入力ピンID
};

/**
 * @struct EditorBTNode
 * @brief エディタ上で描画・操作される「付箋（ノード）」の実体
 */
struct EditorBTNode {
    uint32_t id;               ///< ノードのユニークID
    EditorBTNodeType type;     ///< ノードの種類
    std::string name;          ///< UI上に表示される名前

    // ランタイム用の設定値（インスペクタで編集する）
    ActionID actionOrConditionId = 0;

    // UI用の接続口
    BTPin inputPin;            ///< 親から繋がれる入力ピン
    BTPin outputPin;           ///< 子へ繋ぐための出力ピン（複数の子へ線を伸ばせる）

    // エディタ空間の座標
    float posX = 0.0f;
    float posY = 0.0f;

    // 配置フラグ。これが false の場合、次の描画フレームで座標を強制設定する
    bool positionInitialized = false;

    // 実行順序（Y軸ソート結果）。デバッグ表示用
    int executionOrder = -1;

    // グループノード用のサイズ（インスペクタで変更可能にする）
    float width = 400.0f;
    float height = 400.0f;

    float paramValue1 = 0.0f; // 例: 突進速度
    std::string targetKey = "Player"; // 例: Blackboardのキー

    // デコレーター設定
    int decoratorType = 0;
    float decoratorParam = 0.0f;

    // ランタイムとエディタを紐付ける魔法の変数
    // Bake時に「フラット配列の何番目になったか」を記憶しておく
    int runtimeFlatIndex = -1;
};

/**
 * @class BehaviorTreeWindow
 * @brief Node Editorライブラリをラップし、ウィンドウ描画とユーザー入力を処理するクラス
 */
class BehaviorTreeWindow : public EditorWindow {
public:
    BehaviorTreeWindow();
    virtual ~BehaviorTreeWindow();

    /**
     * @brief 現在のグラフ状態をJSONオブジェクトとして抽出する（Undo/Redo用）
     * @return シリアライズされたグラフの状態
     */
    nlohmann::json GetGraphStateAsJson();

    /**
     * @brief 渡されたJSONオブジェクトからグラフ状態を強制的に復元する（Undo/Redo用）
     * @param state 復元元のJSON状態
     */
    void RestoreGraphState(const nlohmann::json& state);

protected:
    /**
     * @brief ウィンドウ全体の描画の入り口。ImGuiのColumnsを分けて左右のペインを描画する。
     */
    virtual void DrawContents(EditorContext& context) override;

private:
    /**
     * @brief ウィンドウ上部のツールバーを描画し、保存・ロードなどのボタン入力を処理する
     */
    void DrawToolbar();

    /**
     * @brief 左側の広大なノードエディタ空間（キャンバス）を描画し、ピンの接続・切断・ドラッグ操作を処理する
     * @param context 選択中のEntity情報などを取得するためのエディタコンテキスト
     */
    void DrawNodeWorkspace(EditorContext& context);

    /**
     * @brief 右側のインスペクタパネルを描画し、選択中ノードの詳細パラメータを編集させる
     */
    void DrawInspector();

    /**
     * @brief キャンバス上で右クリックした際のコンテキストメニュー（新規ノード追加等）を処理する
     */
    void HandleContextMenu();

    /**
     * @brief ノードを削除し、それに繋がっているリンクも安全に連鎖削除する（ダングリング防止）
     * @param nodeId 削除対象のノードID
     */
    void DeleteNodeAndLinks(uint32_t nodeId);

    /**
     * @brief X座標（左から右）を元に、子ノードの実行優先順序（[1], [2]...）を再計算する
     */
    void UpdateExecutionOrder();

    /**
     * @brief 現在選択されている複数のノードと内部リンクを丸ごと複製（コピー＆ペースト）する
     */
    void DuplicateSelectedNodes();

    /**
     * @brief ユニークな新しいIDを発行する
     */
    uint32_t GetNextId() { return ++_idCounter; }

    /**
     * @brief キャンバス上の指定座標に新しいノードを生成する
     * @param type 生成するノードの種類
     * @param name デフォルトの表示名
     * @param x キャンバス上のX座標
     * @param y キャンバス上のY座標
     */
    void CreateNode(EditorBTNodeType type, const std::string& name, float x, float y);

    /**
     * @brief JSONファイルからエディタのグラフ状態を読み込む
     */
    void LoadGraph(const std::string& path);

    /**
     * @brief 現在のグラフ状態をJSONに保存し、同時にランタイム用の配列（Bake）を統合して書き出す
     */
    void SaveGraph(const std::string& path);

    /**
     * @brief エディタでの操作が行われた際、直前の状態をUndoスタックに記録する
     * @param commandName 履歴に表示される操作名（"Add Node" など）
     */
    void RecordStateForUndo(const std::string& commandName);

    /**
     * @brief 幅優先探索（BFS）を用いて、エディタのグラフ構造からランタイム向けの高速なフラット配列（JSON）を構築する
     * @return 構築された "nodes" 配列のJSONオブジェクト
     */
    nlohmann::json BuildBakedNodes();

private:
    ax::NodeEditor::EditorContext* _nodeContext = nullptr; ///< Node Editorの内部状態（キャンバスのズーム等）

    std::vector<EditorBTNode> _nodes; ///< 画面上の全付箋（ノード）
    std::vector<BTLink> _links;       ///< 画面上の全接続線（リンク）
    uint32_t _idCounter = 1;          ///< IDを被らせないためのカウンター

    std::string _currentFilePath = "Assets/AI/BossAI_Graph.json"; ///< 現在開いているファイルのパス
    bool _needInitialZoom = true;     ///< 初回起動時にカメラを全体に合わせるためのフラグ

    bool _executionOrderDirty = true; ///< 毎フレームのソートループを防ぐための最適化フラグ
    nlohmann::json _lastSavedState;   ///< 直前のグラフ状態（Undo判定用）

    ImVec2 _newNodeSpawnPos = { 0.0f, 0.0f }; ///< 右クリックした瞬間のスクリーン座標

    bool _autoFollow = false;         ///< 実行中ノードへの自動追従フラグ
    uint32_t _lastTrackedNodeId = 0;  ///< 最後にカメラが追従したノードID
};