// src/Engine/Graphics/Model.cpp
#include "Model.h"
#include "TextureManager.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <cmath>
#include <cstdio>

// tinygltf implementation
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// Disable warnings for external library
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4189) // local variable is initialized but not referenced
#pragma warning(disable: 4244) // conversion from 'type1' to 'type2', possible loss of data
#pragma warning(disable: 4267) // conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable: 4996) // deprecated functions
#endif
#include "../../externals/tinygltf/tiny_gltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Assimp implementation
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Model::Model() : dxCommon_(nullptr) {}

Model::~Model() {
	// 頂点リソースの解放（Unmapは不要 - 頂点データは永続的にマップされていない）
	if (vertexResource_) {
		vertexResource_.Reset();
	}
}

void Model::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
}

void Model::LoadFromObj(const std::string& directoryPath, const std::string& filename) {
	// モデルデータの読み込み
	modelData_ = LoadObjFile(directoryPath, filename);

	// モデルデータを最適化（UV球などの表示品質向上のため）
	// ファイル名も渡すように修正
	// OptimizeTriangles(modelData_, filename);  // パフォーマンス向上のため一時的に無効化

	// テクスチャの読み込み
	if (!modelData_.material.textureFilePath.empty()) {
		// テクスチャパスをログに出力
		OutputDebugStringA(("Model: Texture path from MTL: " + modelData_.material.textureFilePath + "\n").c_str());

		// テクスチャが存在するかチェック
		DWORD fileAttributes = GetFileAttributesA(modelData_.material.textureFilePath.c_str());
		if (fileAttributes != INVALID_FILE_ATTRIBUTES) {
			// テクスチャが存在する場合のみ読み込み
			TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
			OutputDebugStringA(("Model: Texture loaded - " + modelData_.material.textureFilePath + "\n").c_str());
		}
		else {
			// テクスチャが見つからない場合、別の場所を探す
			OutputDebugStringA(("WARNING: Texture file not found at: " + modelData_.material.textureFilePath + "\n").c_str());

			// ファイル名のみを抽出
			std::string filenameOnly = modelData_.material.textureFilePath;
			size_t lastSlash = filenameOnly.find_last_of("/\\");
			if (lastSlash != std::string::npos) {
				filenameOnly = filenameOnly.substr(lastSlash + 1);
			}

			// 複数の可能性のある場所を探索
			std::vector<std::string> possiblePaths = {
				"Resources/textures/" + filenameOnly,
				directoryPath + "/" + filenameOnly,
				"Resources/" + filenameOnly,
				"Resources/models/" + filenameOnly
			};

			bool found = false;
			for (const auto& path : possiblePaths) {
				OutputDebugStringA(("Model: Trying alternative path: " + path + "\n").c_str());
				if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
					// 見つかった場合はパスを更新して読み込み
					modelData_.material.textureFilePath = path;
					TextureManager::GetInstance()->LoadTexture(path);
					OutputDebugStringA(("Model: Texture found and loaded from: " + path + "\n").c_str());
					found = true;
					break;
				}
			}

			if (!found) {
				// どこにも見つからない場合
				OutputDebugStringA("WARNING: Texture file not found in any location. Clearing texture path.\n");
				modelData_.material.textureFilePath = ""; // パスをクリア
			}
		}
	}
	else {
		OutputDebugStringA("Model: No texture specified in MTL file\n");
	}

	// 頂点バッファの作成
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	// 頂点バッファビューの設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点データの書き込み
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
	vertexResource_->Unmap(0, nullptr);

	// デバッグ情報
	OutputDebugStringA(("Model: Loaded " + std::to_string(modelData_.vertices.size()) + " vertices from " + filename + "\n").c_str());
}

void Model::LoadFromGltf(const std::string& directoryPath, const std::string& filename) {
	// モデルデータの読み込み
	modelData_ = LoadGltfFile(directoryPath, filename);

	// マルチマテリアル対応の頂点バッファとインデックスバッファを作成
	if (!modelData_.matVertexData.empty()) {
		// マルチマテリアルモード
		size_t meshCount = modelData_.matVertexData.size();
		vertexResources_.resize(meshCount);
		vertexBufferViews_.resize(meshCount);
		indexResources_.resize(meshCount);
		indexBufferViews_.resize(meshCount);

		size_t meshIndex = 0;
		for (const auto& matData : modelData_.matVertexData) {
			// 頂点バッファ作成
			vertexResources_[meshIndex] = dxCommon_->CreateBufferResource(
				sizeof(VertexData) * matData.second.vertices.size());

			vertexBufferViews_[meshIndex].BufferLocation = vertexResources_[meshIndex]->GetGPUVirtualAddress();
			vertexBufferViews_[meshIndex].SizeInBytes = static_cast<UINT>(sizeof(VertexData) * matData.second.vertices.size());
			vertexBufferViews_[meshIndex].StrideInBytes = sizeof(VertexData);

			VertexData* vertexData = nullptr;
			vertexResources_[meshIndex]->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
			std::memcpy(vertexData, matData.second.vertices.data(), sizeof(VertexData) * matData.second.vertices.size());
			vertexResources_[meshIndex]->Unmap(0, nullptr);

			// インデックスバッファ作成
			indexResources_[meshIndex] = dxCommon_->CreateBufferResource(
				sizeof(uint32_t) * matData.second.indices.size());

			indexBufferViews_[meshIndex].BufferLocation = indexResources_[meshIndex]->GetGPUVirtualAddress();
			indexBufferViews_[meshIndex].SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * matData.second.indices.size());
			indexBufferViews_[meshIndex].Format = DXGI_FORMAT_R32_UINT;

			uint32_t* indexData = nullptr;
			indexResources_[meshIndex]->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
			std::memcpy(indexData, matData.second.indices.data(), sizeof(uint32_t) * matData.second.indices.size());
			indexResources_[meshIndex]->Unmap(0, nullptr);

			meshIndex++;
		}

		// マテリアルテンプレートリソースの作成
		materialTemplateResources_.resize(modelData_.materialTemplates.size());
		for (size_t i = 0; i < modelData_.materialTemplates.size(); i++) {
			materialTemplateResources_[i] = dxCommon_->CreateBufferResource(sizeof(MaterialTemplate));
			MaterialTemplate* data = nullptr;
			materialTemplateResources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&data));
			*data = modelData_.materialTemplates[i];
			materialTemplateResources_[i]->Unmap(0, nullptr);
		}

		OutputDebugStringA(("Model: Loaded GLTF with " + std::to_string(meshCount) + " meshes from " + filename + "\n").c_str());
	}
}

// 頂点バッファの作成（継承クラス用）
void Model::CreateVertexBuffer() {
	assert(dxCommon_);

	if (modelData_.vertices.empty()) {
		OutputDebugStringA("Model::CreateVertexBuffer - Warning: No vertices to create buffer\n");
		return;
	}

	// マルチマテリアル対応: マテリアルごとの頂点バッファを作成
	if (!modelData_.matVertexData.empty() && !modelData_.materials.empty()) {
		// マルチマテリアルモード
		OutputDebugStringA(("Model::CreateVertexBuffer - Creating " +
			std::to_string(modelData_.matVertexData.size()) + " vertex buffers for multi-material\n").c_str());

		vertexResources_.clear();
		vertexBufferViews_.clear();

		for (const auto& matData : modelData_.matVertexData) {
			const MaterialVertexData& matVertexData = matData.second;

			if (matVertexData.vertices.empty()) {
				OutputDebugStringA("Model::CreateVertexBuffer - Warning: Empty vertices for material, skipping\n");
				continue;
			}

			// このマテリアル用の頂点バッファを作成
			Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource =
				dxCommon_->CreateBufferResource(sizeof(VertexData) * matVertexData.vertices.size());

			// 頂点バッファビューの設定
			D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
			vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
			vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * matVertexData.vertices.size());
			vertexBufferView.StrideInBytes = sizeof(VertexData);

			// 頂点データの書き込み
			VertexData* vertexDataPtr = nullptr;
			vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataPtr));
			std::memcpy(vertexDataPtr, matVertexData.vertices.data(), sizeof(VertexData) * matVertexData.vertices.size());
			vertexResource->Unmap(0, nullptr);

			vertexResources_.push_back(vertexResource);
			vertexBufferViews_.push_back(vertexBufferView);

			OutputDebugStringA(("Model::CreateVertexBuffer - Created buffer with " +
				std::to_string(matVertexData.vertices.size()) + " vertices\n").c_str());
		}

		OutputDebugStringA(("Model::CreateVertexBuffer - Total " +
			std::to_string(vertexResources_.size()) + " buffers created\n").c_str());
	}
	else {
		// シングルマテリアルモード（従来通り）
		vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());

		// 頂点バッファビューの設定
		vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData_.vertices.size());
		vertexBufferView_.StrideInBytes = sizeof(VertexData);

		// 頂点データの書き込み
		VertexData* vertexData = nullptr;
		vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
		vertexResource_->Unmap(0, nullptr);

		OutputDebugStringA(("Model::CreateVertexBuffer - Created single buffer for " +
			std::to_string(modelData_.vertices.size()) + " vertices\n").c_str());
	}
}

// UV球などの表示品質を向上させるためのモデルデータ最適化関数
void Model::OptimizeTriangles(ModelData& modelData, const std::string& filename) {
	// 最適化前の頂点数を保存
	size_t originalVertexCount = modelData.vertices.size();

	// 重複頂点の検出と削除のためのデータ構造
	std::vector<VertexData> optimizedVertices;
	std::vector<uint32_t> indices;
	std::unordered_map<std::string, uint32_t> vertexMap;

	// 各頂点を処理
	for (const auto& vertex : modelData.vertices) {
		// 頂点のハッシュキーを作成（類似頂点の統合を強化）
		// 精度を大幅に下げて頂点統合を促進（小数点以下2桁に制限）
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
			vertex.position.x, vertex.position.y, vertex.position.z,
			vertex.normal.x, vertex.normal.y, vertex.normal.z,
			vertex.texcoord.x, vertex.texcoord.y);
		std::string key = buffer;

		// この頂点がまだ追加されていなければ追加する
		if (vertexMap.find(key) == vertexMap.end()) {
			vertexMap[key] = static_cast<uint32_t>(optimizedVertices.size());

			// 法線を正規化して品質を向上
			VertexData normalizedVertex = vertex;
			float length = std::sqrt(
				normalizedVertex.normal.x * normalizedVertex.normal.x +
				normalizedVertex.normal.y * normalizedVertex.normal.y +
				normalizedVertex.normal.z * normalizedVertex.normal.z
			);

			if (length > 0.0001f) {
				normalizedVertex.normal.x /= length;
				normalizedVertex.normal.y /= length;
				normalizedVertex.normal.z /= length;
			}

			optimizedVertices.push_back(normalizedVertex);
		}

		// インデックスを追加
		indices.push_back(vertexMap[key]);
	}

	// 法線平均化処理を追加（共有頂点の法線を平均化して滑らかにする）
	std::vector<Vector3> smoothedNormals(optimizedVertices.size(), { 0.0f, 0.0f, 0.0f });
	std::vector<int> normalCount(optimizedVertices.size(), 0);

	// 各三角形の法線を集計
	for (size_t i = 0; i < indices.size(); i += 3) {
		if (i + 2 < indices.size()) {
			// 三角形の頂点インデックス
			uint32_t idx0 = indices[i];
			uint32_t idx1 = indices[i + 1];
			uint32_t idx2 = indices[i + 2];

			// 三角形の辺ベクトル
			Vector3 edge1 = {
				optimizedVertices[idx1].position.x - optimizedVertices[idx0].position.x,
				optimizedVertices[idx1].position.y - optimizedVertices[idx0].position.y,
				optimizedVertices[idx1].position.z - optimizedVertices[idx0].position.z
			};

			Vector3 edge2 = {
				optimizedVertices[idx2].position.x - optimizedVertices[idx0].position.x,
				optimizedVertices[idx2].position.y - optimizedVertices[idx0].position.y,
				optimizedVertices[idx2].position.z - optimizedVertices[idx0].position.z
			};

			// 外積で面法線を計算
			Vector3 faceNormal = {
				edge1.y * edge2.z - edge1.z * edge2.y,
				edge1.z * edge2.x - edge1.x * edge2.z,
				edge1.x * edge2.y - edge1.y * edge2.x
			};

			// 法線の長さを計算
			float length = std::sqrt(
				faceNormal.x * faceNormal.x +
				faceNormal.y * faceNormal.y +
				faceNormal.z * faceNormal.z
			);

			// 法線を正規化
			if (length > 0.0001f) {
				faceNormal.x /= length;
				faceNormal.y /= length;
				faceNormal.z /= length;

				// 各頂点に面法線を加算
				smoothedNormals[idx0].x += faceNormal.x;
				smoothedNormals[idx0].y += faceNormal.y;
				smoothedNormals[idx0].z += faceNormal.z;
				normalCount[idx0]++;

				smoothedNormals[idx1].x += faceNormal.x;
				smoothedNormals[idx1].y += faceNormal.y;
				smoothedNormals[idx1].z += faceNormal.z;
				normalCount[idx1]++;

				smoothedNormals[idx2].x += faceNormal.x;
				smoothedNormals[idx2].y += faceNormal.y;
				smoothedNormals[idx2].z += faceNormal.z;
				normalCount[idx2]++;
			}
		}
	}

	// 法線を平均化
	for (size_t i = 0; i < optimizedVertices.size(); i++) {
		if (normalCount[i] > 0) {
			smoothedNormals[i].x /= normalCount[i];
			smoothedNormals[i].y /= normalCount[i];
			smoothedNormals[i].z /= normalCount[i];

			// 長さを正規化
			float length = std::sqrt(
				smoothedNormals[i].x * smoothedNormals[i].x +
				smoothedNormals[i].y * smoothedNormals[i].y +
				smoothedNormals[i].z * smoothedNormals[i].z
			);

			if (length > 0.0001f) {
				smoothedNormals[i].x /= length;
				smoothedNormals[i].y /= length;
				smoothedNormals[i].z /= length;
			}

			// 平滑化された法線を適用
			optimizedVertices[i].normal = smoothedNormals[i];
		}
	}

	// UV球の場合は特別な処理（UV座標から理論的な法線を計算）
	bool isSphere = (filename.find("sphere") != std::string::npos) ||
		(filename.find("ball") != std::string::npos) ||
		(filename.find("globe") != std::string::npos);

	if (isSphere) {
		for (size_t i = 0; i < optimizedVertices.size(); i++) {
			// UV座標から球面上の位置を計算
			float u = optimizedVertices[i].texcoord.x;
			float v = optimizedVertices[i].texcoord.y;

			// 球面座標
			float phi = u * 2.0f * 3.14159265f;  // 0～2π
			float theta = v * 3.14159265f;       // 0～π

			// 球面座標から理論的な法線を計算
			Vector3 theoreticalNormal;
			theoreticalNormal.x = std::sin(theta) * std::cos(phi);
			theoreticalNormal.y = std::cos(theta);
			theoreticalNormal.z = std::sin(theta) * std::sin(phi);

			// 99%理論的な法線を使用して完全な球面を強制
			Vector3 blendedNormal;
			blendedNormal.x = 0.99f * theoreticalNormal.x + 0.01f * optimizedVertices[i].normal.x;
			blendedNormal.y = 0.99f * theoreticalNormal.y + 0.01f * optimizedVertices[i].normal.y;
			blendedNormal.z = 0.99f * theoreticalNormal.z + 0.01f * optimizedVertices[i].normal.z;

			// 長さを正規化
			float length = std::sqrt(
				blendedNormal.x * blendedNormal.x +
				blendedNormal.y * blendedNormal.y +
				blendedNormal.z * blendedNormal.z
			);

			if (length > 0.0001f) {
				blendedNormal.x /= length;
				blendedNormal.y /= length;
				blendedNormal.z /= length;
			}

			// 混合された法線を適用
			optimizedVertices[i].normal = blendedNormal;
		}
	}

	// インデックスリストから頂点配列を再構築
	std::vector<VertexData> rebuiltVertices;
	for (size_t i = 0; i < indices.size(); i += 3) {
		// 三角形の頂点を追加（順序を維持）
		if (i + 2 < indices.size()) {
			rebuiltVertices.push_back(optimizedVertices[indices[i + 2]]);
			rebuiltVertices.push_back(optimizedVertices[indices[i + 1]]);
			rebuiltVertices.push_back(optimizedVertices[indices[i]]);
		}
	}

	// 最適化された頂点配列で元の配列を置き換え
	if (!rebuiltVertices.empty()) {
		modelData.vertices = rebuiltVertices;
	}
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	// ファイル読み込み
	std::ifstream file(directoryPath + "/" + filename); // fileを開く
	assert(file.is_open()); // 開けなかったら止める

	OutputDebugStringA(("Model: Loading OBJ file: " + directoryPath + "/" + filename + "\n").c_str());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= -1;
			positions.push_back(position);
		}
		else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1 - texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1;
			normals.push_back(normal);
		}
		else if (identifier == "f") {
			VertexData triangle[3];
			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置・UV・法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				triangle[faceVertex] = { position, texcoord, normal };
			}
			// 頂点を逆順で登録することで、周り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
		else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;

			// MTLファイル名をログに出力
			OutputDebugStringA(("Model: Found MTL reference: " + materialFilename + "\n").c_str());

			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの

	// ファイルのフルパス
	std::string mtlPath = directoryPath + "/" + filename;
	std::ifstream file(mtlPath); // ファイルを開く

	// ファイルが開けなかった場合は警告を出力して、デフォルト値を返す
	if (!file.is_open()) {
		OutputDebugStringA(("WARNING: Failed to open MTL file - " + mtlPath + "\n").c_str());
		return materialData;
	}

	OutputDebugStringA(("Model: Successfully opened MTL file - " + mtlPath + "\n").c_str());

	while (std::getline(file, line)) {
		std::string identifier;
		std::stringstream s(line);
		s >> identifier;

		// identifierの応じた処理
		if (identifier == "map_Kd") {
			std::string token;
			std::string textureFilename;

			// オプション引数（-s, -o, -t など）を解析してファイル名を見つける
			while (s >> token) {
				if (token[0] == '-') {
					// オプション引数の場合
					if (token == "-s") {
						// スケールオプション：3つの値を読み取る
						float scaleU, scaleV, scaleW;
						s >> scaleU >> scaleV >> scaleW;
						materialData.textureScale.x = scaleU;
						materialData.textureScale.y = scaleV;
						OutputDebugStringA(("MTL Parser: Texture scale: " + std::to_string(scaleU) + ", " + std::to_string(scaleV) + "\n").c_str());
					}
					else if (token == "-o") {
						// オフセットオプション：3つの値を読み取る
						float offsetU, offsetV, offsetW;
						s >> offsetU >> offsetV >> offsetW;
						materialData.textureOffset.x = offsetU;
						materialData.textureOffset.y = offsetV;
						OutputDebugStringA(("MTL Parser: Texture offset: " + std::to_string(offsetU) + ", " + std::to_string(offsetV) + "\n").c_str());
					}
					else if (token == "-t") {
						// タービュレンスオプション：3つの値をスキップ
						std::string dummy1, dummy2, dummy3;
						s >> dummy1 >> dummy2 >> dummy3;
					}
					else if (token == "-mm") {
						// -mmオプションは2つの値をスキップ
						std::string dummy1, dummy2;
						s >> dummy1 >> dummy2;
					}
					else {
						// その他のオプションは1つの値をスキップ
						std::string dummy;
						s >> dummy;
					}
				}
				else {
					// オプションではない場合、これがテクスチャファイル名
					textureFilename = token;
					break;
				}
			}

			// テクスチャファイル名をログに出力
			OutputDebugStringA(("MTL Parser: Found texture reference: " + textureFilename + "\n").c_str());

			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;

			// フルパスをログに出力
			OutputDebugStringA(("MTL Parser: Full texture path constructed: " + materialData.textureFilePath + "\n").c_str());
		}
		else if (identifier == "Ka") {
			// ambient color
			s >> materialData.ambient.x >> materialData.ambient.y >> materialData.ambient.z;
			materialData.ambient.w = 1.0f;
		}
		else if (identifier == "Kd") {
			// diffuse color
			s >> materialData.diffuse.x >> materialData.diffuse.y >> materialData.diffuse.z;
			materialData.diffuse.w = 1.0f;
		}
		else if (identifier == "Ks") {
			// specular color
			s >> materialData.specular.x >> materialData.specular.y >> materialData.specular.z;
			materialData.specular.w = 1.0f;
		}
		else if (identifier == "Ns") {
			// shininess
			s >> materialData.shininess;
		}
		else if (identifier == "d" || identifier == "Tr") {
			// transparency (d) or transparency inverted (Tr)
			if (identifier == "d") {
				s >> materialData.alpha;
			}
			else { // Tr (transparency inverted)
				float tr;
				s >> tr;
				materialData.alpha = 1.0f - tr;
			}
		}
	}

	return materialData;
}

// URLデコード関数（%エンコードされた文字列をデコード）
static std::string UrlDecode(const std::string& str) {
	std::string result;
	for (size_t i = 0; i < str.length(); i++) {
		if (str[i] == '%' && i + 2 < str.length()) {
			// %XX形式の16進数をデコード
			std::string hex = str.substr(i + 1, 2);
			char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
			result += ch;
			i += 2;
		}
		else {
			result += str[i];
		}
	}
	return result;
}

ModelData Model::LoadGltfFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;

	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

	if (!scene || !scene->HasMeshes()) {
		OutputDebugStringA(("ERROR: Failed to load GLTF file: " + filePath + "\n").c_str());
		OutputDebugStringA("Loading fallback model instead\n");
		return LoadObjFile("Resources/Debug/obj", "box.obj");
	}

	OutputDebugStringA(("Model: Loading GLTF file: " + filePath + "\n").c_str());
	OutputDebugStringA(("Model: Found " + std::to_string(scene->mNumMeshes) + " meshes\n").c_str());

	// マルチマテリアル対応: 各メッシュを処理
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];

		// メッシュ名を取得（UTF-8からワイド文字列に変換）
		std::string utf8 = mesh->mName.C_Str();
		int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		std::wstring meshName(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &meshName[0], len);

		if (!mesh->HasNormals() || !mesh->HasTextureCoords(0)) {
			OutputDebugStringA(("WARNING: Mesh missing normals or texture coordinates: " + utf8 + "\n").c_str());
			continue;
		}

		MaterialVertexData matVertexData;
		matVertexData.vertices.resize(mesh->mNumVertices);
		matVertexData.materialIndex = mesh->mMaterialIndex;

		OutputDebugStringA(("Model: Mesh[" + std::to_string(meshIndex) + "] \"" + utf8 +
			"\" uses material index " + std::to_string(mesh->mMaterialIndex) +
			", " + std::to_string(mesh->mNumVertices) + " vertices\n").c_str());

		// 頂点データの読み込み
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

			// 右手系→左手系への変換
			matVertexData.vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };
			matVertexData.vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
			matVertexData.vertices[vertexIndex].texcoord = { texcoord.x, texcoord.y };

			// 全体の頂点リストにも追加
			modelData.vertices.push_back(matVertexData.vertices[vertexIndex]);
		}

		// インデックスの解析
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			if (face.mNumIndices != 3) {
				OutputDebugStringA("WARNING: Non-triangulated face found\n");
				continue;
			}

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				matVertexData.indices.push_back(vertexIndex);
				modelData.indices.push_back(vertexIndex);
			}
		}

		modelData.matVertexData[meshName] = matVertexData;
	}

	// マテリアルの読み込み
	if (scene->mNumMaterials == 0) {
		// デフォルトマテリアルを作成
		MaterialData matData;
		matData.textureFilePath = "Resources/Debug/white1x1.png";
		TextureManager::GetInstance()->LoadTexture(matData.textureFilePath);

		MaterialTemplate matTemplate;
		matTemplate.metallic = 0.0f;

		modelData.materials.push_back(matData);
		modelData.materialTemplates.push_back(matTemplate);

		// 単一マテリアルフィールドにも設定
		modelData.material = matData;
	}
	else {
		OutputDebugStringA(("Model: Loading " + std::to_string(scene->mNumMaterials) + " materials\n").c_str());

		for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
			aiMaterial* material = scene->mMaterials[materialIndex];
			MaterialData matData;
			MaterialTemplate matTemplate;

			// ベースカラーテクスチャの読み込み
			if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
				aiString textureFilePath;
				material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);

				// URLデコードを適用（%エンコードされたパスを修正）
				std::string decodedPath = UrlDecode(textureFilePath.C_Str());
				matData.textureFilePath = directoryPath + "/" + decodedPath;

				// テクスチャファイルが存在するか確認
				DWORD fileAttributes = GetFileAttributesA(matData.textureFilePath.c_str());
				if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
					// ファイルが見つからない場合、ファイル名のみを抽出して別の場所を探す
					std::string filenameOnly = decodedPath;
					size_t lastSlash = filenameOnly.find_last_of("/\\");
					if (lastSlash != std::string::npos) {
						filenameOnly = filenameOnly.substr(lastSlash + 1);
					}

					// 代替パスを試す
					std::vector<std::string> possiblePaths = {
						directoryPath + "/" + filenameOnly,
						"Resources/textures/" + filenameOnly,
						"Resources/" + filenameOnly
					};

					bool found = false;
					for (const auto& path : possiblePaths) {
						if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
							matData.textureFilePath = path;
							found = true;
							OutputDebugStringA(("Model: Found texture at alternative path: " + path + "\n").c_str());
							break;
						}
					}

					if (!found) {
						OutputDebugStringA(("WARNING: Texture not found: " + matData.textureFilePath + ", using white1x1\n").c_str());
						matData.textureFilePath = "Resources/Debug/white1x1.png";
					}
				}

				// テクスチャ読み込み
				TextureManager::GetInstance()->LoadTexture(matData.textureFilePath);
				OutputDebugStringA(("Model: Loaded texture: " + matData.textureFilePath + "\n").c_str());
			}
			else {
				// テクスチャがない場合はwhite1x1を使用
				matData.textureFilePath = "Resources/Debug/white1x1.png";
				TextureManager::GetInstance()->LoadTexture(matData.textureFilePath);
			}

			// メタリックファクターの取得
			float metallic = 0.0f;
			if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
				matTemplate.metallic = metallic;
				OutputDebugStringA(("Model: Material metallic factor: " + std::to_string(metallic) + "\n").c_str());
			}
			else {
				matTemplate.metallic = 0.0f;
			}

			modelData.materials.push_back(matData);
			modelData.materialTemplates.push_back(matTemplate);

			OutputDebugStringA(("Model: Material[" + std::to_string(materialIndex) + "] texture: " +
				matData.textureFilePath + "\n").c_str());

			// 最初のマテリアルを単一マテリアルフィールドにも設定
			if (materialIndex == 0) {
				modelData.material = matData;
			}
		}
	}

	OutputDebugStringA(("Model: Loaded " + std::to_string(modelData.materials.size()) + " materials total\n").c_str());
	OutputDebugStringA(("Model: Loaded " + std::to_string(modelData.matVertexData.size()) + " meshes total\n").c_str());
	return modelData;
}

