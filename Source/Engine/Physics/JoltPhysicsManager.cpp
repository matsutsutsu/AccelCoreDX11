#include "JoltPhysicsManager.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <cstdarg>
#include <iostream>
#include <windows.h>

#include <Jolt/Physics/Body/BodyManager.h>

using namespace JPH;

#define PhysicsPerformanceCounters 1 // パフォーマンスを最大限対応にする
//#define PhysicsPerformanceCounters 0 // パフォーマンスを最低限対応にする

// --- デバッグ用コールバック ---
static void TraceImpl(const char *inFMT, ...)
{
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    OutputDebugStringA("[Jolt] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(
    const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
    char buffer[2048];
    snprintf(buffer,
        sizeof(buffer),
        "[Jolt Assert] %s:%d: (%s) %s\n",
        inFile,
        inLine,
        inExpression,
        (inMessage ? inMessage : ""));

    // ★確実にVisual Studioの出力ウィンドウにエラーを叩き出す
    OutputDebugStringA(buffer);

    return true; // trueでデバッガをブレークさせる
}
#endif

// --- フィルター実装 ---
BPLayerInterfaceImpl::BPLayerInterfaceImpl()
{
    // 「ObjectLayerのMOVING」を持った奴は、「BroadPhaseのMOVINGバケツ」に入れろ！という指示
    mObjectToBroadPhase[PhysicsLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[PhysicsLayers::MOVING]     = BroadPhaseLayers::MOVING;
}
uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const { return BroadPhaseLayers::NUM_LAYERS; }
BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(ObjectLayer inLayer) const
{
    JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char *BPLayerInterfaceImpl::GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const
{
    switch ((BroadPhaseLayer::Type)inLayer) {
    case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
    case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
    default: JPH_ASSERT(false); return "INVALID";
    }
}
#endif

bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(
    ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const
{
    switch (inLayer1) {
    case PhysicsLayers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
    case PhysicsLayers::MOVING: return true;
    default: JPH_ASSERT(false); return false;
    }
}

bool ObjectLayerPairFilterImpl::ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const
{
    switch (inObject1) {
    case PhysicsLayers::NON_MOVING: 
        return inObject2 == PhysicsLayers::MOVING; // 壁は、動くモノ(MOVING)とだけ当たる
    case PhysicsLayers::MOVING: 
        return true;                               // 動くモノは、全員と当たる
    default: JPH_ASSERT(false); 
        return false;
    }
}

// --- JoltPhysicsManager 実装 ---

JoltPhysicsManager::~JoltPhysicsManager()
{
    if (!m_isInitialized) return;

    // 解放順序は「作成の逆」でなければ絶対にクラッシュします
    // Systemを先に開放しないと、Systemの中で使っているAllocatorやFilterが先に消えてしまい、
    // Systemのデストラクタでアクセス違反になる
    m_physicsSystem.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    UnregisterTypes();

    delete Factory::sInstance;
    Factory::sInstance = nullptr;
}

void JoltPhysicsManager::Initialize()
{
    if (m_isInitialized) return;

    // 1. 基盤コールバックの設定
    RegisterDefaultAllocator();
    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

    // 2. ファクトリと型の登録
    Factory::sInstance = new Factory();
    RegisterTypes();

    // 3. 一時アロケータ (10MB) と ジョブシステムの構築
    // TempAllocatorは毎フレーム出る衝突点のリストなどの一時的なデータを
    // 確保するためで毎回newせずに、フレームの最後に一括でリセットして再利用するスタイル 
    
    #if PhysicsPerformanceCounters
    // 50 * 1024 * 1024 = 約50MBの計算用スクラッチメモリ
    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(50 * 1024 * 1024);

    #else

    m_tempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);
    #endif


    // ジョブシステムは、物理計算を複数のCPUコアで並列に走らせるためのもの
    m_jobSystem     = std::make_unique<JobSystemThreadPool>(
        cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    // 4. 物理システムのパラメータ設定
    
    #if PhysicsPerformanceCounters

    // 4. 物理システムのパラメータ設定（群衆対応のAAAスケール）
    const uint cMaxBodies             = 65536; // 最大6万体まで許容
    const uint cNumBodyMutexes        = 0;     // デフォルトのミューテックス数
    const uint cMaxBodyPairs          = 65536; // 密集によるペア爆発に備える
    const uint cMaxContactConstraints = 20480; // 大量に押し合うための接点バッファ

    #else

    // 世界に存在できる最大オブジェクト数
    // 上限を決めることで内部のデータ構造を効率化しているため、必ず指定する必要があります
    const uint cMaxBodies             = 1024; // サンプル用。本番は65536等に増やす
    // ボディのロックに使うミューテックスの数（0で自動設定）。多いほど並列処理が効率化しますが、メモリ使用量も増えます。
    const uint cNumBodyMutexes        = 0;
    // 衝突ペアの上限。これも大きいほど多くの衝突を処理できますが、メモリ使用量が増えます。
    const uint cMaxBodyPairs          = 1024;
    // 衝突制約の上限。これも大きいほど多くの衝突を処理できますが、メモリ使用量が増えます。
    const uint cMaxContactConstraints = 1024;

    #endif

    m_physicsSystem = std::make_unique<PhysicsSystem>();
    m_physicsSystem->Init(cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        m_bpLayerInterface,
        m_objectVsBroadphaseFilter,
        m_objectVsObjectFilter);

    // 5. 重力設定（下方向に-9.8）
    m_physicsSystem->SetGravity(Vec3(0, -9.8f, 0));

    // =========================================================
    //  物理世界に「接触リスナー（ポスト）」を設置する
    // =========================================================
    m_contactListener = std::make_unique<JoltContactListener>();
    m_physicsSystem->SetContactListener(m_contactListener.get());

    m_isInitialized = true;
}

// 毎フレーム呼び出す更新関数
void JoltPhysicsManager::Step(float deltaTime)
{
    if (!m_isInitialized) return;

    const int cCollisionSteps = 1;
    m_physicsSystem->Update(deltaTime, cCollisionSteps, m_tempAllocator.get(), m_jobSystem.get());
}

void JoltPhysicsManager::DrawBodies(ShapeRenderer *shapeRenderer)
{
    if (!m_isInitialized || !shapeRenderer) return;

    // レンダラーがまだ作られていなければ生成する
    if (!m_debugRenderer) {
        m_debugRenderer = std::make_unique<JoltDebugRenderer>(shapeRenderer);
    }

    // Joltの描画設定（何を描画するか）
    JPH::BodyManager::DrawSettings settings;
    settings.mDrawBoundingBox = false; // 四角い大枠（AABB）を描画するか
    settings.mDrawShape       = true;  // 本当の当たり判定を描画するか
    settings.mDrawVelocity    = false; // 速度の矢印を描画するか

    // 物理空間内の全ボディを描画する命令を、ShapeRendererに向けて発行！
    m_physicsSystem->DrawBodies(settings, m_debugRenderer.get());
}


