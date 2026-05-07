#include "SceneManager.h"
#include "SceneFactory.h"

#include "Game/Scene/SceneGame.h"


#include "Engine/Serialization/SceneSerializer.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/SerializationContext.h"

// タグとシステム
#include "Engine/GamePlay/Core/PersistentTag.h"
#include "Engine/GamePlay/Transform/TransformUpdateSystem.h"
#include "Engine/GamePlay/Transform/HierarchyCleanupSystem.h"
#include "Engine/GamePlay/Physics/Jolt/JoltCleanupSystem.h"



void SceneManager::Initialize()
{
    // 初期シーンの登録と遷移リクエストをここで行います
    SceneFactory::Instance().Register("Game", []() { return new SceneGame(); });
    ChangeScene("Game");
}

void SceneManager::FixedUpdate(float fixedTime)
{
    if (nextScene == nullptr && currentScene != nullptr) {
        currentScene->FixedUpdate(fixedTime);
    }
}

void SceneManager::Update(float elapsedTime, int frameIndex, DX12System* dx12System, SystemDataContext* systemDataContext, ResourceManager* resourceManager)
{
    if (nextScene != nullptr)
    {
        Clear();

        currentScene = nextScene;
        nextScene = nullptr;

        // シーン初期化処理にDX12システムを渡す
        if (!currentScene->IsReady()) {
            currentScene->Initialize(dx12System, systemDataContext, resourceManager);
            currentScene->SetReady();
        }

        currentScene->Start();
    }

    if (currentScene != nullptr) {
        currentScene->Update(elapsedTime, frameIndex);
    }
}

void SceneManager::Render(DX12System* dx12System, CommandList* commandList, int frameIndex)
{
    if (currentScene != nullptr) {
        currentScene->Render(dx12System, commandList, frameIndex);
    }
}

void SceneManager::Clear()
{
    if (currentScene != nullptr) {
        currentScene->Finalize();
        delete currentScene;
        currentScene = nullptr;
    }
}

void SceneManager::ChangeScene(const std::string& name)
{
    Scene* newScene = SceneFactory::Instance().Create(name);
    if (newScene) {
        ChangeScene(newScene);
        _currentSceneName = name;
    }
}

void SceneManager::ChangeScene(Scene* scene)
{
    if (nextScene != nullptr) {
        delete nextScene;
    }
    nextScene = scene;
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

void SceneManager::LoadScene(CCL::ECS::Core::World* world, const std::string& filepath)
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

