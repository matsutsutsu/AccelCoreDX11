#include <stdlib.h>
#include <fstream>
#include <functional>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include<filesystem>
#include "GLTFImporter.h"

#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include <winsock.h>


#include "ModelResource.h"

const std::vector<D3D11_INPUT_ELEMENT_DESC> ModelResource::InputElementDescs =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },	
	{ "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

// CEREALバージョン定義
CEREAL_CLASS_VERSION(ModelResource::Node, 1)
CEREAL_CLASS_VERSION(ModelResource::Material, 1)
CEREAL_CLASS_VERSION(ModelResource::Subset, 1)
CEREAL_CLASS_VERSION(ModelResource::Vertex, 1)
CEREAL_CLASS_VERSION(ModelResource::Mesh, 1)
CEREAL_CLASS_VERSION(ModelResource::VectorKeyframe, 1)
CEREAL_CLASS_VERSION(ModelResource::QuaternionKeyframe, 1)
CEREAL_CLASS_VERSION(ModelResource::NodeAnim, 1)
CEREAL_CLASS_VERSION(ModelResource::Animation, 1)
CEREAL_CLASS_VERSION(ModelResource, 1)

// シリアライズ
namespace DirectX
{
	template<class Archive>
	void serialize(Archive& archive, XMUINT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4X4& m)
	{
		archive(
			cereal::make_nvp("_11", m._11), cereal::make_nvp("_12", m._12), cereal::make_nvp("_13", m._13), cereal::make_nvp("_14", m._14),
			cereal::make_nvp("_21", m._21), cereal::make_nvp("_22", m._22), cereal::make_nvp("_23", m._23), cereal::make_nvp("_24", m._24),
			cereal::make_nvp("_31", m._31), cereal::make_nvp("_32", m._32), cereal::make_nvp("_33", m._33), cereal::make_nvp("_34", m._34),
			cereal::make_nvp("_41", m._41), cereal::make_nvp("_42", m._42), cereal::make_nvp("_43", m._43), cereal::make_nvp("_44", m._44)
		);
	}
}

template<class Archive>
void ModelResource::Node::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(id),
		CEREAL_NVP(name),
		CEREAL_NVP(path),
		CEREAL_NVP(parentIndex),
		CEREAL_NVP(scale),
		CEREAL_NVP(rotate),
		CEREAL_NVP(translate)
	);
}

template<class Archive>
void ModelResource::Material::serialize(Archive& ar, int version)
{
	ar(CEREAL_NVP(name),
		CEREAL_NVP(baseTextureFileName),
		CEREAL_NVP(normalTextureFileName),
		CEREAL_NVP(emissiveTextureFileName),
		CEREAL_NVP(occlusionTextureFileName),
		CEREAL_NVP(metalnessRoughnessTextureFileName),
		CEREAL_NVP(baseColor),
		CEREAL_NVP(emissiveColor),
		CEREAL_NVP(metalness),
		CEREAL_NVP(roughness),
		CEREAL_NVP(occlusionStrength),
		CEREAL_NVP(alphaCutoff),
		CEREAL_NVP(alphaMode));
}


template<class Archive>
void ModelResource::Subset::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(startIndex),
		CEREAL_NVP(indexCount),
		CEREAL_NVP(materialIndex)
	);
}

template<class Archive>
void ModelResource::Vertex::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(position),
		CEREAL_NVP(normal),
		CEREAL_NVP(tangent),
		CEREAL_NVP(texcoord),
		CEREAL_NVP(boneWeight),
		CEREAL_NVP(boneIndex)
	);
}

template<class Archive>
void ModelResource::Bone::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(offsetTransform)
	);
}

template<class Archive>
void ModelResource::Mesh::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(vertices),
		CEREAL_NVP(indices),
		CEREAL_NVP(subsets),
		CEREAL_NVP(bones),
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(nodeIndices),
		CEREAL_NVP(materialIndex),
		CEREAL_NVP(offsetTransforms),
		CEREAL_NVP(boundsMin),
		CEREAL_NVP(boundsMax)
	);
}

template<class Archive>
void ModelResource::VectorKeyframe::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void ModelResource::QuaternionKeyframe::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void ModelResource::NodeAnim::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(positionKeyframes),
		CEREAL_NVP(rotationKeyframes),
		CEREAL_NVP(scaleKeyframes)
	);
}

 

template<class Archive>
void ModelResource::Animation::serialize(Archive& archive, int version)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(secondsLength),
		CEREAL_NVP(nodeAnims)
	);
}

// 読み込み
// cerealがあればDeserializeで読み込み、
// なければ汎用GLTFImporterで読み込み
void ModelResource::Load(ID3D11Device* device, const char* filename)
{
	// ディレクトリパス取得
	char drive[32], dir[256], dirname[256];
	::_splitpath_s(filename, drive, sizeof(drive), dir, sizeof(dir), nullptr, 0, nullptr, 0);
	::_makepath_s(dirname, sizeof(dirname), drive, dir, nullptr, nullptr);

	std::filesystem::path filepath(filename);
	std::filesystem::path dirpath(filepath.parent_path());
	std::filesystem::path extension = filepath.extension();

	//// 独自形式のモデルファイルの存在確認
	filepath.replace_extension(".cereal");
	if (std::filesystem::exists(filepath))
	{
		// 独自形式のモデルファイルの読み込み
		Deserialize(filepath.string().c_str());
	}
	else if (extension == ".gltf" || extension == ".glb")
	{
		// 汎用モデルファイルの読み込み
		// GLTFImporterを使用してモデルデータをリソース用に読み込む
		// これはデータをCPUメモリ上に読み込むだけで、GPUリソースは作成しない
		GLTFImporter importer(filename);

		// マテリアルデータ読み取り
		importer.LoadMaterials(materials, nullptr);

		// ノードデータ読み取り
		importer.LoadNodes(nodes);

		// メッシュデータ読み取り
		importer.LoadMeshes(meshes, nodes);

		// アニメーションデータ読み取り
		importer.LoadAnimations(animations, nodes, 30.0f); //samplerate（第3引数）はお好み

		//独自形式のモデルファイルを保存
		Serialize(filepath.string().c_str());
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}


	// モデル構築
	BuildModel(device, dirname);

	// モデル全体のAABBを計算
	ComputeModelLocalAABB();

}


// アニメーション追加読み込み
void ModelResource::AppendAnimations(const char* filename)
{
	std::filesystem::path filepath(filename);
	std::filesystem::path dirpath(filepath.parent_path());

	if (filepath.extension() == ".gltf" ||
		filepath.extension() == ".glb")
	{
		// 汎用モデルファイルの読み込み
		GLTFImporter importer(filename);

		// アニメーションデータ読み取り
		importer.LoadAnimations(animations, nodes);

	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}
}

void ModelResource::ComputeMeshBounds(Mesh& mesh)
{
	using namespace DirectX;

	XMFLOAT3 minV = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
	XMFLOAT3 maxV = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (const Vertex& v : mesh.vertices)
	{
		minV.x = (std::min)(minV.x, v.position.x);
		minV.y = (std::min)(minV.y, v.position.y);
		minV.z = (std::min)(minV.z, v.position.z);

		maxV.x = (std::max)(maxV.x, v.position.x);
		maxV.y = (std::max)(maxV.y, v.position.y);
		maxV.z = (std::max)(maxV.z, v.position.z);
	}

	mesh.boundsMin = minV;
	mesh.boundsMax = maxV;
}

// モデル構築
// 実際にGPUリソースを作成する場所
void ModelResource::BuildModel(ID3D11Device* device, const char* dirname)
{
	for (Material& material : materials)
	{
		// 共通テクスチャローダ
		auto loadTexture = [&](const std::string& textureFileName,
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV)
			{
				if (textureFileName.empty())
				{
					// ダミーテクスチャを作成
					HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF, outSRV.GetAddressOf());
					_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
					return;
				}

				// パス解決
				char fullpath[256];
				::_makepath_s(fullpath, 256, nullptr, dirname, textureFileName.c_str(), nullptr);

				// マルチバイト→ワイド文字変換
				wchar_t wfilename[256];
				::MultiByteToWideChar(CP_ACP, 0, fullpath, -1, wfilename, 256);

				// 実際にテクスチャをロード
				HRESULT hr = GpuResourceUtils::LoadTexture(device, fullpath, outSRV.GetAddressOf());
				if (FAILED(hr))
				{
					// ロード失敗時は白テクスチャを代用
					hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF, outSRV.GetAddressOf());
					_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
				}
			};

		// 各マップをロード
		loadTexture(material.baseTextureFileName, material.baseMap);
		loadTexture(material.normalTextureFileName, material.normalMap);
		loadTexture(material.emissiveTextureFileName, material.emissiveMap);
		loadTexture(material.occlusionTextureFileName, material.occlusionMap);
		loadTexture(material.metalnessRoughnessTextureFileName, material.metalnessRoughnessMap);
	}


	for (Mesh& mesh : meshes)
	{
		// サブセット
		for (Subset& subset : mesh.subsets)
		{
			subset.material = &materials.at(subset.materialIndex);
		}

		// 頂点バッファ
		{
			D3D11_BUFFER_DESC bufferDesc = {};
			D3D11_SUBRESOURCE_DATA subresourceData = {};

			bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * mesh.vertices.size());
			//bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;
			subresourceData.pSysMem = mesh.vertices.data();
			subresourceData.SysMemPitch = 0;
			subresourceData.SysMemSlicePitch = 0;

			/*HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));*/



			HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
			if (FAILED(hr)) {
				printf(" VertexBuffer creation failed: hr=0x%08X, vertexCount=%zu\n", hr, mesh.vertices.size());
				mesh.vertexBuffer.Reset(); // 安全にnullへ
			}
			else {
				auto d = mesh.vertices.size();
				auto e = bufferDesc.ByteWidth;
				int a = 1;
			}
		}

		// インデックスバッファ
		{
			D3D11_BUFFER_DESC bufferDesc = {};
			D3D11_SUBRESOURCE_DATA subresourceData = {};

			bufferDesc.ByteWidth = static_cast<UINT>(sizeof(u_int) * mesh.indices.size());
			//bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;
			subresourceData.pSysMem = mesh.indices.data();
			subresourceData.SysMemPitch = 0; //Not use for index buffers.
			subresourceData.SysMemSlicePitch = 0; //Not use for index buffers.
			HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.indexBuffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}

		// バウンディング計算
		ComputeMeshBounds(mesh);
	}
}

// モデル全体のAABBを計算
void ModelResource::ComputeModelLocalAABB()
{
    using namespace DirectX;

    XMFLOAT3 minP = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
    XMFLOAT3 maxP = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    bool hasMesh = false;
    for (const Mesh& mesh : meshes)
    {
        if (mesh.vertices.empty() && mesh.indices.empty())
            continue;

        hasMesh = true;

        minP.x = (std::min)(minP.x, mesh.boundsMin.x);
        minP.y = (std::min)(minP.y, mesh.boundsMin.y);
        minP.z = (std::min)(minP.z, mesh.boundsMin.z);

        maxP.x = (std::max)(maxP.x, mesh.boundsMax.x);
        maxP.y = (std::max)(maxP.y, mesh.boundsMax.y);
        maxP.z = (std::max)(maxP.z, mesh.boundsMax.z);
    }

    if (hasMesh)
    {
        m_boundingBox.Center = XMFLOAT3((minP.x + maxP.x) * 0.5f,
                                        (minP.y + maxP.y) * 0.5f,
                                        (minP.z + maxP.z) * 0.5f);

        m_boundingBox.Extents = XMFLOAT3((maxP.x - minP.x) * 0.5f,
                                         (maxP.y - minP.y) * 0.5f,
                                         (maxP.z - minP.z) * 0.5f);
    }
    else
    {
        m_boundingBox.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_boundingBox.Extents = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }
}

// シリアライズ
void ModelResource::Serialize(const char* filename)
{
	std::ofstream ostream(filename, std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);
		}
		catch (...)
		{
			char buffer[256];
			sprintf_s(buffer, sizeof(buffer), "model serialize failed.\n%s\n", filename);
			_ASSERT_EXPR_A(false, buffer);
			return;
		}
	}
}

// デシリアライズ
void ModelResource::Deserialize(const char* filename)
{
	std::ifstream istream(filename, std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);
		}
		catch (...)
		{
			char buffer[256];
			sprintf_s(buffer, sizeof(buffer), "model deserialize failed.\n%s\n", filename);
			_ASSERT_EXPR_A(false, buffer);
			return;
		}
	}
	else
	{
		char buffer[256];
		sprintf_s(buffer, sizeof(buffer), "File not found > %s", filename);
		_ASSERT_EXPR_A(false, buffer);
	}
}
