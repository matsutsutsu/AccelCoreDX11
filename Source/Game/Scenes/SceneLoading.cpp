#include "SceneLoading.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/GamePlay/Core/Scene/SceneManager.h"
#include <imgui.h>
#include <wrl/client.h> // CoInitialize用

SceneLoading::SceneLoading(Scene *nextScene) : _nextScene(nextScene) {}

SceneLoading::~SceneLoading()
{
    // 安全のためデストラクタでもスレッド終了を確認
    if (_thread && _thread->joinable()) {
        _thread->join();
        delete _thread;
        _thread = nullptr;
    }
}

void SceneLoading::Initialize()
{
    // BaseScene初期化（カメラやライトの準備）
    // ※ 重い処理はLoadingThreadで行うので、ここは最小限
    BaseScene::Initialize();

    // スレッドを生成し、ローディング処理を非同期で開始する
    _isFinished = false;
    if (_nextScene) {
        _thread = new std::thread(&SceneLoading::LoadingThread, this);
    }
 
}

void SceneLoading::Finalize()
{
    // スレッドの終了待機
    if (_thread) {
        if (_thread->joinable()) {
            _thread->join();
        }
        delete _thread;
        _thread = nullptr;
    }

   
    BaseScene::Finalize();
}

void SceneLoading::Update(float elapsedTime)
{
    time += elapsedTime; 

    BaseScene::Update(elapsedTime);

    // ロード完了チェック
    if (_isFinished) {
        // 次のシーンへ遷移
        // ここで渡す _nextScene は既に Initialize() 済み
        // SceneManager::Update で IsReady() チェックが効くので再初期化されない
        SceneManager::Instance().ChangeScene(_nextScene);

        // 所有権をManagerに渡したのでポインタをクリア
        _nextScene = nullptr;
    }
}

// スレッド処理本体
void SceneLoading::LoadingThread()
{
    // COMライブラリ初期化 (WICテクスチャ読み込み等に必須)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (_nextScene) {
        // ★ ここで次のシーンの重いロード処理が走る ★
        // 注意: DeviceContext(描画コマンド)を触る処理はNG
        // リソース生成(CreateBuffer/CreateTexture)はOK
        _nextScene->Initialize();

        // ロードが終わったのでフラグを立てる
        _nextScene->SetReady();
    }

    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }

    // 完了フラグを立てる
    _isFinished = true;
}

void SceneLoading::OnRenderUI()
{
  
}

void SceneLoading::OnDrawImGui()
{
    // デバッグ用表示
    ImGui::SetNextWindowPos(ImVec2(100, 100));
    ImGui::Begin("Now Loading...", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Loading next scene...");
    ImGui::End();
}