#include <filesystem>
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "GLTFImporter.h"
#include "Model.h"
#include "ResourceManager.h"
#include "Engine/Core/Math/StringHash.h"
#include "Engine/Graphics/Resource/ResourceManager.h"
#include <array>

// コンストラクタ
Model::Model(ID3D11Device* device, const char* filename, float sampleRate)
    : filePath(filename)
{
    // ファイル名が空の場合（動的生成モード）
    // ファイル読み込みを行わず、空のリソースだけ作って終了する
    if (filePath.empty()) {
        // LoadModelResourceではなく、AllocateEmptyModelResourceを呼ぶ。
        // これで、Load()でクラッシュすることも、他の動的モデルとメモリが混ざることも絶対に無くなります。
        resource = ResourceManager::Instance().AllocateEmptyModelResource();
        return;
    }


    resource = ResourceManager::Instance().LoadModelResource(filename);
    if (!resource.IsValid())
    {
        _ASSERT_EXPR(false, L"ModelResource が取得できませんでした。");
        return;
    }

    // ModelResourceに格納された階層情報（Nodes）を
    // Modelのインスタンスにコピーして親子関係を再構築する
    const std::vector<ModelResource::Node>& resNodes = ResourceManager::Instance().GetModel(resource)->GetNodes();
    nodes.resize(resNodes.size());
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const ModelResource::Node& src = resNodes.at(i);
        Node& dst = nodes.at(i);

        if (src.parentIndex >= 0)
        {
            dst.parent = &nodes.at(src.parentIndex);
        }

        dst.name = src.name;
        dst.position = src.translate;
        dst.rotation = src.rotate;
        dst.scale = src.scale;

        dst.children.clear();

        // 行列初期化（単位行列）
        XMStoreFloat4x4(&dst.localTransform, DirectX::XMMatrixIdentity());
        XMStoreFloat4x4(&dst.globalTransform, DirectX::XMMatrixIdentity());
        XMStoreFloat4x4(&dst.worldTransform, DirectX::XMMatrixIdentity());
    }
    // ② 親子関係の再構築
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& src = resNodes[i];
        if (src.parentIndex >= 0 && src.parentIndex < (int)nodes.size())
        {
            nodes[i].parent = &nodes[src.parentIndex];
            nodes[src.parentIndex].children.push_back(&nodes[i]);
        }
    }

    // リソース側のマテリアルを参照
    const std::vector<ModelResource::Material>& resMaterials = ResourceManager::Instance().GetModel(resource)->GetMaterials();
    // モデル側マテリアルを用意
    materials.resize(resMaterials.size());

    for (size_t i = 0; i < resMaterials.size(); ++i)
    {
        const auto& src = resMaterials[i];
        auto& dst = materials[i];

        // ---- 参照を保存 ----
        dst.name = src.name;

        // 純粋なデータの実体を作成する
        dst.data = std::make_shared<MaterialData>();

        // --- データを辞書へマッピング ---
        // デフォルトシェーダーID（必要に応じて変更）
        // アルファモードに応じてシェーダーを切り替える例
        /*
        if (dst.data->alphaMode == AlphaMode::Opaque)
            dst.data->shaderId = ShaderId::Phong; // または Toon
        else
            dst.data->shaderId = ShaderId::Toon; // 半透明対応など
        */
        dst.data->shaderHash = "PBR"_hash; // デフォルト

        // アルファモードのコピー（型変換）
        dst.data->alphaMode = static_cast<AlphaMode>(src.alphaMode);
        dst.data->alphaCutoff = src.alphaCutoff;

        // 1. カラー情報
        // シェーダーリフレクションの変数名と一致させる
        dst.data->colors["materialColor"] = src.baseColor; // 従来のPhong/Basicシェーダー用
        dst.data->colors["baseColor"] = src.baseColor; // 新しいPBRシェーダー用
        // emissiveColor (float3) を float4 に変換して格納
        dst.data->colors["emissiveColor"] = { src.emissiveColor.x, src.emissiveColor.y, src.emissiveColor.z, 1.0f };

        // 2. スカラー情報
        dst.data->scalars["metalness"] = src.metalness;
        dst.data->scalars["roughness"] = src.roughness;
        dst.data->scalars["occlusionStrength"] = src.occlusionStrength;
        dst.data->scalars["alphaCutoff"] = src.alphaCutoff; // シェーダー用にも辞書に入れておく

      
        // 3. ToonShader用のデフォルト値（これがないと黒くなる）
        dst.data->colors["rimColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
        dst.data->scalars["rimPower"] = 5.0f;
        dst.data->scalars["rimIntensity"] = 1.0f;

        // トゥーン用デフォルトランプを辞書に登録
        // キー名 "RampTexture" は HLSL の Texture2D 変数名と一致させること！
        // 今のところこれをやったところで誤差だから問題ない、気になるなら後でデータ駆動にして修正する     
        dst.data->textures["RampTexture"] = ResourceManager::Instance().LoadTexture("Assets/Textures/Shader/ramp.png");

        // OutlineShader用のデフォルト値
        dst.data->colors["color"] = { 0.0f, 0.0f, 0.0f, 1.0f }; // 黒い線
        dst.data->scalars["size"] = 0.02f;                      // 太さ

        // 4. テクスチャ情報
        // シェーダーリフレクションのスロット名と一致させる
        if (src.baseMap) {
            auto handle = ResourceManager::Instance().RegisterTexture(src.baseMap);
            dst.data->textures["DiffuseMap"] = handle; // 従来のPhongシェーダー用
            dst.data->textures["AlbedoMap"] = handle; // 新しいPBRシェーダー用
            dst.data->scalars["useAlbedoMap"] = 1.0f; // ★フラグON
        }
        else {
            dst.data->scalars["useAlbedoMap"] = 0.0f; // ★フラグOFF
        }

        if (src.normalMap) {
            dst.data->textures["NormalMap"] = ResourceManager::Instance().RegisterTexture(src.normalMap);
        }
        else {
            dst.data->scalars["useNormalMap"] = 0.0f;
        }

        if (src.emissiveMap) {
            dst.data->textures["EmissiveMap"] = ResourceManager::Instance().RegisterTexture(src.emissiveMap);
        }

        if (src.occlusionMap) {
            dst.data->textures["OcclusionMap"] = ResourceManager::Instance().RegisterTexture(src.occlusionMap);
        }

        if (src.metalnessRoughnessMap) {
            auto handle = ResourceManager::Instance().RegisterTexture(src.metalnessRoughnessMap);
            dst.data->textures["MetalnessRoughnessMap"] = handle; // 従来用
            dst.data->textures["MetalRoughMap"] = handle; // 新しいPBRシェーダー用
            dst.data->scalars["useMetalRoughMap"] = 1.0f; // ★フラグON
        }
        else {
            dst.data->scalars["useMetalRoughMap"] = 0.0f; // ★フラグOFF
        }


    }

const std::vector<ModelResource::Mesh>& resMeshes = ResourceManager::Instance().GetModel(resource)->GetMeshes();
    meshes.resize(resMeshes.size());
    for (size_t i = 0; i < resMeshes.size(); ++i)
    {
        const auto& src = resMeshes[i];
        auto& dst = meshes[i];

        // ---- 参照を保存 ----
        dst.data = &src;
        dst.material = &materials.at(src.materialIndex);  // ここでComponentを持つMaterialへの参照が入る

    
        // ModelResource::Mesh はそのメッシュが属する nodeIndex を持っている [cite: 15]
        if (src.nodeIndex >= 0 && src.nodeIndex < (int)nodes.size())
        {
            dst.node = &nodes.at(src.nodeIndex);
        }
        else
        {
            dst.node = nullptr;
        }

        dst.bones.reserve(src.bones.size());
        for (const ModelResource::Bone& srcBone : src.bones)
        {
            Model::Bone& dstBone = dst.bones.emplace_back();
            dstBone.data = &srcBone;
            dstBone.node = &nodes.at(srcBone.nodeIndex);
        }
    }




    // 行列初期化
    DirectX::XMFLOAT4X4 worldTransform;
    DirectX::XMStoreFloat4x4(&worldTransform, DirectX::XMMatrixIdentity());
    UpdateTransform(worldTransform);
}

// アニメーションインデックス取得
int Model::GetAnimationIndex(const char* name) const
{
    if (!resource.IsValid()) { return -1; } // IsValidに変更

    const auto& animations = ResourceManager::Instance().GetModel(resource)->GetAnimations();

    for (size_t animationIndex = 0; animationIndex < animations.size(); ++animationIndex)
    {
        if (animations.at(animationIndex).name == name)
        {
            return static_cast<int>(animationIndex);
        }
    }
    return -1;
}

// ノードインデックス取得
int Model::GetNodeIndex(const char* name) const
{
    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        if (nodes.at(nodeIndex).name == name)
        {
            return static_cast<int>(nodeIndex);
        }
    }
    return -1;
}

// トランスフォーム更新処理
void Model::UpdateTransform(const DirectX::XMFLOAT4X4& worldTransform)
{
    DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&worldTransform);

    const std::vector<ModelResource::Node>& resNodes = ResourceManager::Instance().GetModel(resource)->GetNodes();
   

    int index = 0;
    for (Node& node : nodes)
    {
        const ModelResource::Node &n = resNodes[index];
        if (node.rotation.x != n.rotate.x ||
            node.rotation.y != n.rotate.y ||
            node.rotation.z != n.rotate.z ||
            node.rotation.w != n.rotate.w)
        {
            int a = 0;
        }
        index++;

        // ローカル行列算出
        DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
        DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
        DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
        DirectX::XMMATRIX LocalTransform = S * R * T;

        // グローバル行列算出
        DirectX::XMMATRIX ParentGlobalTransform;
        if (node.parent != nullptr)
        {
            ParentGlobalTransform = DirectX::XMLoadFloat4x4(&node.parent->globalTransform);
        }
        else
        {
            ParentGlobalTransform = DirectX::XMMatrixIdentity();
        }
        DirectX::XMMATRIX GlobalTransform = LocalTransform * ParentGlobalTransform;

        // ワールド行列算出
        DirectX::XMMATRIX WorldTransform = GlobalTransform * ParentWorldTransform;

        // 計算結果を格納
        DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);
        DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);
        DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);
    }
}

void Model::ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const
{
   if (!resource.IsValid()) { return; } // IsValidに変更

    const auto& animations = ResourceManager::Instance().GetModel(resource)->GetAnimations();

    const ModelResource::Animation& animation = animations.at(animationIndex);
    const ModelResource::NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);

    // 位置
    for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)
    {
        // 現在の時間がどのキーフレームの間にいるか判定する
        const ModelResource::VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);
        const ModelResource::VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);
        if (time >= keyframe0.seconds && time <= keyframe1.seconds)
        {
            // 再生時間とキーフレームの時間から補完率を算出する
            float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

            // 前のキーフレームと次のキーフレームの姿勢を補完
            DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);
            DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);
            DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);
            // 計算結果をノードに格納
            DirectX::XMStoreFloat3(&nodePose.position, V);
        }
    }
    // 回転
    for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)
    {
        // 現在の時間がどのキーフレームの間にいるか判定する
        const ModelResource::QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);
        const ModelResource::QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);
        if (time >= keyframe0.seconds && time <= keyframe1.seconds)
        {
            // 再生時間とキーフレームの時間から補完率を算出する
            float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

            // 前のキーフレームと次のキーフレームの姿勢を補完
            DirectX::XMVECTOR Q0 = DirectX::XMLoadFloat4(&keyframe0.value);
            DirectX::XMVECTOR Q1 = DirectX::XMLoadFloat4(&keyframe1.value);
            DirectX::XMVECTOR Q = DirectX::XMQuaternionSlerp(Q0, Q1, rate);
            // 計算結果をノードに格納
            DirectX::XMStoreFloat4(&nodePose.rotation, Q);
        }
    }
    // スケール
    for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)
    {
        // 現在の時間がどのキーフレームの間にいるか判定する
        const ModelResource::VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);
        const ModelResource::VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);
        if (time >= keyframe0.seconds && time <= keyframe1.seconds)
        {
            // 再生時間とキーフレームの時間から補完率を算出する
            float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

            // 前のキーフレームと次のキーフレームの姿勢を補完
            DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);
            DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);
            DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);
            // 計算結果をノードに格納
            DirectX::XMStoreFloat3(&nodePose.scale, V);
        }
    }
}

// アニメーション計算
void Model::ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const
{
    if (nodePoses.size() != nodes.size())
    {
        nodePoses.resize(nodes.size());
    }
    for (size_t nodeIndex = 0; nodeIndex < nodePoses.size(); ++nodeIndex)
    {
        ComputeAnimation(animationIndex, static_cast<int>(nodeIndex), time, nodePoses.at(nodeIndex));
    }
}

void Model::ComputeAnimationWithDelta(
    int animationIndex,
    float currentTime,
    float previousTime,
    std::vector<NodePose>& outNodePoses,
    bool extractRootMotion,
    int rootNodeIndex, 
    DirectX::XMVECTOR* outDeltaPosition) const
{
    // 1. 現在の時間で全ノードのポーズを計算
    ComputeAnimation(animationIndex, currentTime, outNodePoses);

    // 2. ルートモーション抽出フラグが有効、かつインデックスが有効な場合
    if (extractRootMotion && rootNodeIndex >= 0 && rootNodeIndex < nodes.size())
    {
        if (outDeltaPosition)
        {
            NodePose prevPose, currPose;
            ComputeAnimation(animationIndex, rootNodeIndex, previousTime, prevPose);
            ComputeAnimation(animationIndex, rootNodeIndex, currentTime, currPose);

            DirectX::XMVECTOR prevPos = DirectX::XMLoadFloat3(&prevPose.position);
            DirectX::XMVECTOR currPos = DirectX::XMLoadFloat3(&currPose.position);
            *outDeltaPosition = DirectX::XMVectorSubtract(currPos, prevPos);
        }

        // 描画キャンセルのため、0フレーム目の位置を取得して固定
        NodePose initialPose;
        ComputeAnimation(animationIndex, rootNodeIndex, 0.0f, initialPose);
        outNodePoses[rootNodeIndex].position = initialPose.position;
    }
    else if (outDeltaPosition)
    {
        *outDeltaPosition = DirectX::XMVectorZero();
    }
}

// ノードポーズ設定
void Model::SetNodePoses(const std::vector<NodePose>& nodePoses)
{
    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const NodePose& pose = nodePoses.at(nodeIndex);
        Node& node = nodes.at(nodeIndex);

        node.position = pose.position;
        node.rotation = pose.rotation;
        node.scale = pose.scale;
    }
}

// ノードポーズ取得
void Model::GetNodePoses(std::vector<NodePose>& nodePoses) const
{
    if (nodePoses.size() != nodes.size())
    {
        nodePoses.resize(nodes.size());
    }
    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const Node& node = nodes.at(nodeIndex);
        NodePose& pose = nodePoses.at(nodeIndex);

        pose.position = node.position;
        pose.rotation = node.rotation;
        pose.scale = node.scale;
    }
}

// アニメーション補間処理
void Model::BlendAnimations(
    const std::vector<NodePose>& animation0,
    const std::vector<NodePose>& animation1,
    float blendRate,
    std::vector<NodePose>& result)
{
    size_t nodeCount = animation0.size();
    result.resize(nodeCount);

    for (size_t i = 0; i < nodeCount; ++i)
    {
        const Model::NodePose& pose0 = animation0[i];
        const Model::NodePose& pose1 = animation1[i];
        Model::NodePose& blendedPose = result[i];

        // 位置の線形補間
        DirectX::XMVECTOR pos0 = DirectX::XMLoadFloat3(&pose0.position);
        DirectX::XMVECTOR pos1 = DirectX::XMLoadFloat3(&pose1.position);
        DirectX::XMVECTOR blendedPos = DirectX::XMVectorLerp(pos0, pos1, blendRate);
        DirectX::XMStoreFloat3(&blendedPose.position, blendedPos);

        // 回転のスフィア線形補間
        DirectX::XMVECTOR rot0 = DirectX::XMLoadFloat4(&pose0.rotation);
        DirectX::XMVECTOR rot1 = DirectX::XMLoadFloat4(&pose1.rotation);
        DirectX::XMVECTOR blendedRot = DirectX::XMQuaternionSlerp(rot0, rot1, blendRate);
        DirectX::XMStoreFloat4(&blendedPose.rotation, blendedRot);

        // スケールの線形補間
        DirectX::XMVECTOR scale0 = DirectX::XMLoadFloat3(&pose0.scale);
        DirectX::XMVECTOR scale1 = DirectX::XMLoadFloat3(&pose1.scale);
        DirectX::XMVECTOR blendedScale = DirectX::XMVectorLerp(scale0, scale1, blendRate);
        DirectX::XMStoreFloat3(&blendedPose.scale, blendedScale);
    }
}


const ModelResource* Model::GetResource() const
{
    return ResourceManager::Instance().GetModel(resource);
}

ModelResource* Model::GetResourceMutable()
{
    return ResourceManager::Instance().GetModel(resource);
}

std::shared_ptr<Model> Model::CreateFromData(ID3D11Device *device,
    const std::vector<ModelResource::Vertex>              &vertices,
    const std::vector<uint32_t>                           &indices,
    std::shared_ptr<MaterialData>                          baseMaterial)
{
    // 1. 空のモデルインスタンスを作成
    // ダミーパスを渡すが、ロード失敗前提で進める
    auto model = std::make_shared<Model>(device, "");

    // リソースを新規作成して上書き
    auto *res = ResourceManager::Instance().GetModel(model->resource);

    // =================================================
    // 2. リソースデータの構築 (ModelResource)
    // =================================================

    // --- Meshの構築 ---
    ModelResource::Mesh mesh;
    mesh.vertices = vertices;
    mesh.indices  = indices;

    // GPUバッファ作成
    // (GpuResourceUtils に追加した関数を使用)
    if (!mesh.vertices.empty()) {
        GpuResourceUtils::CreateVertexBuffer(device,
            mesh.vertices.data(),
            static_cast<UINT>(mesh.vertices.size() * sizeof(ModelResource::Vertex)),
            mesh.vertexBuffer.GetAddressOf());
    }
    if (!mesh.indices.empty()) {
        GpuResourceUtils::CreateIndexBuffer(device,
            mesh.indices.data(),
            static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t)),
            mesh.indexBuffer.GetAddressOf());
    }

    // メンバ変数の設定
    mesh.nodeIndex     = 0; // ルートノードに紐付け
    mesh.materialIndex = 0; // 0番目のマテリアルを使用

    // Subsetの設定 (ここが重要：構造体定義に合わせる)
    ModelResource::Subset subset;
    subset.startIndex    = 0; // indexStart ではなく startIndex
    subset.indexCount    = static_cast<UINT>(mesh.indices.size());
    subset.materialIndex = 0;
    subset.material      = nullptr; // リソース内ではポインタ解決前なのでnull
    mesh.subsets.push_back(subset);

    // バウンディングボックス計算
    mesh.boundsMin = {FLT_MAX, FLT_MAX, FLT_MAX};
    mesh.boundsMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (mesh.vertices.empty()) {
        mesh.boundsMin = {0, 0, 0};
        mesh.boundsMax = {0, 0, 0};
    }
    else {
        for (const auto &v : mesh.vertices) {
            mesh.boundsMin.x = (std::min)(mesh.boundsMin.x, v.position.x);
            mesh.boundsMin.y = (std::min)(mesh.boundsMin.y, v.position.y);
            mesh.boundsMin.z = (std::min)(mesh.boundsMin.z, v.position.z);
            mesh.boundsMax.x = (std::max)(mesh.boundsMax.x, v.position.x);
            mesh.boundsMax.y = (std::max)(mesh.boundsMax.y, v.position.y);
            mesh.boundsMax.z = (std::max)(mesh.boundsMax.z, v.position.z);
        }
    }

    // メッシュをリソースに登録
    std::vector<ModelResource::Mesh> resMeshes = {mesh};
    res->SetMeshes(resMeshes);

    // --- Materialの構築 ---
    ModelResource::Material resMat;
    resMat.name = "BatchMaterial";
    // 必要に応じてデフォルト値を設定
    resMat.baseColor                                  = {1.0f, 1.0f, 1.0f, 1.0f};
    resMat.roughness                                  = 0.8f;
    std::vector<ModelResource::Material> resMaterials = {resMat};
    res->SetMaterials(resMaterials);

    // --- Nodeの構築 ---
    ModelResource::Node resNode;
    resNode.name                              = "Root";
    resNode.parentIndex                       = -1;
    resNode.scale                             = {1, 1, 1};
    resNode.rotate                            = {0, 0, 0, 1};
    resNode.translate                         = {0, 0, 0};
    std::vector<ModelResource::Node> resNodes = {resNode};
    res->SetNodes(resNodes);

    // 全体AABB計算
    res->ComputeModelLocalAABB();

    // =================================================
    // 3. モデルインスタンスの構築 (Model)
    // コンストラクタで行われるはずの処理を手動で行う
    // =================================================

    // --- Nodeの同期 ---
    model->nodes.resize(1);
    model->nodes[0].name = "Root";
    DirectX::XMStoreFloat4x4(&model->nodes[0].localTransform, DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&model->nodes[0].globalTransform, DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&model->nodes[0].worldTransform, DirectX::XMMatrixIdentity());
    model->nodes[0].position = {0, 0, 0};
    model->nodes[0].scale    = {1, 1, 1};
    model->nodes[0].rotation = {0, 0, 0, 1};

    // --- Materialの同期 ---
    model->materials.resize(1);
    model->materials[0].name = "BatchMaterial";

    // ★修正: 引数で渡されたマテリアルデータを共有する！
    // これによりテクスチャやシェーダー設定がそのまま引き継がれます
    if (baseMaterial) {
        model->materials[0].data = baseMaterial;
    }
    else {
        // マテリアルが無い場合（フォールバック）のみ、光りすぎないPBR初期値を設定する
        auto matData = std::make_shared<MaterialData>();

        matData->shaderHash = "PBR"_hash;
        matData->colors["materialColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
        matData->colors["baseColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
        matData->colors["emissiveColor"] = { 0.0f, 0.0f, 0.0f, 1.0f };

        // PBRの「ちょうどいい」デフォルト値
        matData->scalars["metalness"] = 0.0f; // 非金属
        matData->scalars["roughness"] = 0.8f; // 光りすぎないよう、少しざらつかせる
        matData->scalars["occlusionStrength"] = 1.0f;
        matData->scalars["rimPower"] = 5.0f;
        matData->scalars["rimIntensity"] = 0.0f;

        // テクスチャは無いので全てオフ
        matData->colors["useAlbedoMap"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        matData->colors["useNormalMap"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        matData->colors["useMetalRoughMap"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        matData->colors["useOcclusionMap"] = { 0.0f, 0.0f, 0.0f, 0.0f };

        model->materials[0].data = matData;
    }

    return model;
}