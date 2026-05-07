#include"SceneManager.h"
#include"SceneFactory.h"

// シーン登録ヘッダー
#include"Game/Scenes/EditorScene.h"
#include "Game/Scenes/ParticleScene.h"


#include "Engine/Serialization/SceneSerializer.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/SerializationContext.h"

// タグとシステム
#include "Engine/GamePlay/Core/PersistentTag.h"
#include "Engine/GamePlay/Transform/TransformUpdateSystem.h"
#include "Engine/GamePlay/Transform/HierarchyCleanupSystem.h"
#include "Engine/GamePlay/Physics/Jolt/JoltCleanupSystem.h"


#include "tracy/Tracy.hpp"


using namespace CCL::ECS::Core;

//初期化
void SceneManager::Initialize()
{
    //シーン登録
    //SceneFactory::Instance().Register("ModelViewer", []() { return new ModelViewerScene(); });
    SceneFactory::Instance().Register("Editor", []() { return new EditorScene(); });

    SceneFactory::Instance().Register("Particle", []() { return new ParticleScene(); });


    // 最初に表示するシーン
    ChangeScene("Editor");
    //ChangeScene("Particle");
}

// --- 追加：固定更新の実装 ---
void SceneManager::FixedUpdate(float fixedTime)
{
    // シーン遷移待ちの状態では物理演算を行わないのが安全
    if (nextScene == nullptr && currentScene != nullptr)
    {
        currentScene->FixedUpdate(fixedTime);
    }
}

//更新処理
void SceneManager::Update(float elapsedTime)
{

    if (nextScene != nullptr)
    {
        //古いシーンを終了処理
        Clear();

        //新しいシーンを設定
        currentScene = nextScene;
        nextScene = nullptr;

        //シーン初期化処理
        // 【1段階目】: 重いデータロード (Initialize)
        // もし SceneLoading を経由して既に行われていれば、IsReady() が true
        // なのでスキップされる
        if (!currentScene->IsReady()) {
            currentScene->Initialize();

            // 終わった瞬間にマネージャーが責任を持ってフラグを立てる
            currentScene->SetReady();
        }

        // 【2段階目】: GPUへのセットアップ (Start) 
        // これは絶対にメインスレッドのこのタイミングで1回だけ呼ばれる。
        // dc->Map や UIManager の dc へのバインド等はここで行う。
        currentScene->Start();
    }

    if (currentScene != nullptr)
    {
        currentScene->Update(elapsedTime);
    }

}

//描画処理
void SceneManager::Render()
{
    if (currentScene != nullptr)
    {
        currentScene->Render();
    }
}

//GUI描画
void SceneManager::DrawGUI()
{
    if (currentScene != nullptr)
    {
        currentScene->DrawGUI();
    }
}

//シーンクリア
void SceneManager::Clear()
{
    if (currentScene != nullptr)
    {
        currentScene->Finalize();
        delete currentScene;
        currentScene = nullptr;
    }
}


// =========================================================================
// ECSシーンの保存 (Save)
// =========================================================================
void SceneManager::SaveScene(CCL::ECS::Core::World* world, const std::string& filepath)
{
    // 1. 写真撮影前の「世界の整理」
    // 今溜まっている RequestDestroy などのペンディング操作を完全に終わらせ、
    // 中途半端な半死半生のエンティティがセーブデータに混ざるのを防ぐ
    world->ScrutinyAndApply();

    // 2. 翻訳家（シリアライザ）に現在の状態をJSONに書き出させる
    SceneSerializer::Serialize(world, filepath);
}

void SceneManager::LoadScene(World* world, const std::string& filepath)
{
    // ==========================================================
    // 1. 破壊の宣告 (Marking)
    // ==========================================================
    auto& chunks = world->GetChunkManager().GetChunks();
    for (auto& chunkPtr : chunks) {
        if (!chunkPtr) continue;
        auto* chunk = chunkPtr.get();

        // PersistentTag（シーンを跨ぐシステム等）は絶対に破壊しない
        if (chunk->HasComponent<Tag::PersistentTag>()) continue;

        for (size_t i = 0; i < chunk->GetEntityCount(); ++i) {
            CCL::ECS::EntityID entity = chunk->GetEntityByIndex(i);
            if (entity != CCL::ECS::InvalidEntityID) {
                // 階層ごと安全に削除予約（DestroyTag を付与）
                TransformUpdateSystem::MarkForDestruction(*world, entity, true);
            }
        }
    }

    // ==========================================================
    // 2. 同期的な解体作業 (Synchronous Cleanup)
    // ここでシステムを直接呼ぶことで、1フレーム待たずに「今すぐ」物理空間とメモリを消し去る
    // ==========================================================

    // ① Joltの剛体を即座に消去する
    JoltCleanupSystem::ExecuteCleanup(*world);

    // ② ECSの階層を切り離し、RequestDestroy を発行する
    HierarchyCleanupSystem::ExecuteCleanup(*world);

    // ③ 世界に溜まった RequestDestroy を「今すぐ確定」させる
    world->ScrutinyAndApply();

    // ③ 【超重要】世界に溜まった RequestDestroy を「今すぐ確定」させ、メモリを完全に解放する！
    world->ScrutinyAndApply();

    // ==========================================================
    // 3. まっさらな世界にシーンを構築 (Build)
    // ==========================================================
    std::vector<CCL::ECS::EntityID> createdEntities = SceneSerializer::Deserialize(world, filepath);

    // ==========================================================
    // 4. 記憶の翻訳 (ID Remapping)
    // ==========================================================
    auto idMap = SerializationContext::GetIdMap(); // (※辞書を取得する仕組みを想定)
    for (CCL::ECS::EntityID newEntity : createdEntities) {
        for (const auto& pair : ComponentRegistry::Instance().GetAllMeta()) {
            if (pair.second.remap) {
                pair.second.remap(world, newEntity, idMap);
            }
        }
    }
}


//シーン切り替え
void SceneManager::ChangeScene(const std::string& name)
{
    // 名前を記録しておく
    _currentSceneName = name;

    Scene* newScene = SceneFactory::Instance().Create(name);
    if (newScene) {
        nextScene = newScene;
    }
}


void SceneManager::ChangeScene(Scene* scene)
{
    // ※ここでは _currentSceneName は自動更新されない
    // 呼び出し側（GateSystem）で SetCurrentSceneName を呼んでからこれを使う
    if (nextScene != nullptr) {
        delete nextScene;
    }

    // nextSceneに直接セット
    nextScene = scene;
}