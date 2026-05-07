#pragma once

#include <string>
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <map>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "Engine/Graphics/Resource/GraphicsTypes.h"

class ModelResource
{
public:
	ModelResource() {}
	virtual ~ModelResource() {}

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;

	using NodeId = UINT64;

	struct Node
	{
		NodeId				id;           // 識別用の一意なID
		std::string			name;         // ノード名（例："Head"）
		std::string			path;         // モデル内での階層パス
		int					parentIndex = -1;  // 親ノードのインデックス（-1 ならルート）

		// ノードの初期Transform情報（ローカル座標）
		DirectX::XMFLOAT3	scale = { 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT4	rotate = { 0.0f, 0.0f, 0.0f, 1.0f }; // クォータニオンの単位元
		DirectX::XMFLOAT3	translate = { 0.0f, 0.0f, 0.0f };

		template<class Archive>
		void serialize(Archive& archive, int version);


	};

	// スキニング用のボーン情報
	struct Bone
	{
		int nodeIndex;						// 対応するノードのインデックス
		DirectX::XMFLOAT4X4 offsetTransform; // モデル空間からボーン空間への変換行列

		template<class Archive>
		void serialize(Archive& archive, int version);
	};


	// テクスチャ・カラーなどのマテリアル情報
	struct Material
	{
		std::string name;	// マテリアル名

		// 各種テクスチャファイル名
		std::string baseTextureFileName;
		std::string normalTextureFileName;
		std::string emissiveTextureFileName;
		std::string occlusionTextureFileName;
		std::string metalnessRoughnessTextureFileName;

		// 基本的なマテリアルパラメータ
		DirectX::XMFLOAT4 baseColor = { 1, 1, 1, 1 };
		DirectX::XMFLOAT3 emissiveColor = { 1, 1, 1 };
		float metalness = 0.0f;
		float roughness = 0.0f;
		float occlusionStrength = 0.0f;
		float alphaCutoff = 0.5f;
		AlphaMode alphaMode = AlphaMode::Opaque;

		// DirectXリソース（読み込まれたテクスチャの実体）
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissiveMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> occlusionMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalnessRoughnessMap;


		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	// １つのメッシュ内で、異なるマテリアルを適用する部分
	struct Subset
	{
		UINT startIndex = 0;    // 描画開始インデックス
		UINT indexCount = 0;    // このサブセットのインデックス数
		int materialIndex = 0;  // 使用するマテリアルのインデックス

		Material* material = nullptr; // マテリアル参照（実体は上のMaterial配列）

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	// 頂点データ構造
	struct Vertex
	{
		DirectX::XMFLOAT3	position = { 0, 0, 0 };   // 頂点位置
		DirectX::XMFLOAT3	normal = { 0, 0, 0 };     // 法線
		DirectX::XMFLOAT4	tangent = { 0, 0, 0, 0 }; // 接線
		DirectX::XMFLOAT2	texcoord = { 0, 0 };      // UV座標

		// スキニング情報
		DirectX::XMFLOAT4	boneWeight = { 1, 0, 0, 0 }; // 各ボーンの影響度
		DirectX::XMUINT4	boneIndex = { 0, 0, 0, 0 };  // ボーンインデックス


		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	// モデルのジオメトリ情報
	struct Mesh
	{
		std::vector<Vertex>	vertices;   // 頂点配列
		std::vector<UINT>	indices;    // インデックス配列
		std::vector<Subset>	subsets;    // マテリアルごとのサブセット
		std::vector<Bone>	bones;      // このメッシュに影響を与えるボーン群

		int nodeIndex;					// このメッシュがぶら下がるノードのインデックス
		int materialIndex;				// メッシュ全体のマテリアル
		std::vector<int> nodeIndices;	// 複数ノードに紐づく場合（スキニングなど）
		std::vector<DirectX::XMFLOAT4X4> offsetTransforms; // 各ボーンのオフセット行列

		// バウンディング情報（当たり判定やカリング用）
		DirectX::XMFLOAT3 boundsMin;
		DirectX::XMFLOAT3 boundsMax;


		// DirectXの頂点・インデックスバッファ
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;


		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	// アニメーション関連構造体

	// ベクトル（位置・スケール）キーフレーム
	struct VectorKeyframe
	{
		float					seconds;
		DirectX::XMFLOAT3		value;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	// クォータニオン（回転）キーフレーム
	struct QuaternionKeyframe
	{
		float					seconds;
		DirectX::XMFLOAT4		value;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	// ノードごとのアニメーションデータ
	struct NodeAnim
	{
		std::vector<VectorKeyframe>		positionKeyframes;
		std::vector<QuaternionKeyframe>	rotationKeyframes;
		std::vector<VectorKeyframe>		scaleKeyframes;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct Animation
	{
		std::string					name;
		float						secondsLength;
		std::vector<NodeAnim>		nodeAnims;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};


	// 各種データ取得
	const std::vector<Mesh>& GetMeshes() const { return meshes; }
	const std::vector<Node>& GetNodes() const { return nodes; }
	const std::vector<Animation>& GetAnimations() const { return animations; }
        // ★追加: 書き込み可能 (エディタ用)
        std::vector<Animation>      &GetAnimationsMutable() { return animations; }
	const std::vector<Material>& GetMaterials() const { return materials; }

	void SetMeshes(const std::vector<Mesh>& m) { meshes = m; }
	void SetNodes(const std::vector<Node>& n) { nodes = n; }
	void SetMaterials(const std::vector<Material>& m) { materials = m; }
	void SetAnimations(const std::vector<Animation>& a) { animations = a; }

	// 読み込み
	void Load(ID3D11Device* device, const char* filename);

	// アニメーション追加読み込み
	void AppendAnimations(const char* filename);

	// メッシュのバウンディング計算
	void ComputeMeshBounds(Mesh& mesh);

	void ComputeModelLocalAABB();

	// モデル全体のバウンディング取得
	const DirectX::BoundingBox& GetBoundingBox() const { return m_boundingBox; }


protected:
	// モデルセットアップ
	void BuildModel(ID3D11Device* device, const char* dirname);

	// シリアライズ
	void Serialize(const char* filename);

	// デシリアライズ
	void Deserialize(const char* filename);



protected:
	std::vector<Node>		nodes;
	std::vector<Material>	materials;
	std::vector<Mesh>		meshes;
	std::vector<Animation>	animations;

	DirectX::BoundingBox m_boundingBox; // モデル全体のローカルAABB
};

