#pragma once
#include <assert.h>
#include <cmath>
#include <stdio.h>
#include <vector>
#include <string>
#include <map>

// ===== Vector2 =====
struct Vector2
{
    float x;
    float y;
};

// ===== Vector3 =====
struct Vector3
{
    float x;
    float y;
    float z;

    Vector3 operator+(const Vector3& other) const {
        return Vector3{ x + other.x, y + other.y, z + other.z };
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3{ x - other.x, y - other.y, z - other.z };
    }

    Vector3 operator*(float scalar) const {
        return Vector3{ x * scalar, y * scalar, z * scalar };
    }

    Vector3 operator/(float scalar) const {
        return Vector3{ x / scalar, y / scalar, z / scalar };
    }

    Vector3& operator+=(const Vector3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vector3& operator-=(const Vector3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vector3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    Vector3& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }
};

// ===== Vector4 =====
struct Vector4
{
	float x;
	float y;
	float z;
	float w;
};

// Vector4のドット積
float Dot(const Vector4& v1, const Vector4& v2);

// Vector4の正規化
Vector4 Normalize(const Vector4& v);

inline Vector4 operator-(const Vector4& q1) {
	Vector4 result;

	result.w = -q1.w;
	result.x = -q1.x;
	result.y = -q1.y;
	result.z = -q1.z;
	return result;
}
inline Vector4 operator+(Vector4 q1, Vector4 q2) {
	q1.w += q2.w;
	q1.x += q2.x;
	q1.y += q2.y;
	q1.z += q2.z;
	return q1;
}

inline Vector4 operator*(float n, Vector4 q1) {
	Vector4 result;

	result.w = q1.w * n;
	result.x = q1.x * n;
	result.y = q1.y * n;
	result.z = q1.z * n;
	return result;
}

// ===== Matrix4x4 =====
struct Matrix4x4 {
	float m[4][4];
};

//float Cot(float theta);

Matrix4x4 MakeIdentity4x4();
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
Matrix4x4 MakeRotateMatrix(const Vector3& rotate);
Matrix4x4 MakeRotateMatrix(const Vector4& rotate);
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector4& rotate, const Vector3& translate);
Matrix4x4 Inverse(const Matrix4x4& m);
//Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
//Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearclip, float farclip);
//Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
Matrix4x4 MakeRotateXMatrix(float radian);
Matrix4x4 MakeRotateYMatrix(float radian);
Matrix4x4 MakeRotateZMatrix(float radian);
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
Matrix4x4 Transpose(const Matrix4x4& m);

// 補間関数
Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t);
Vector4 Slerp(const Vector4& q1, const Vector4& q2, float t);

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct Material {
    // Base Color (従来のcolorと同等)
    Vector4 baseColorFactor;
    
    // Metallic & Roughness
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    
    // Emissive
    Vector3 emissiveFactor;
    float alphaCutoff;
    
    // Flags
    int32_t hasBaseColorTexture;
    int32_t hasMetallicRoughnessTexture;
    int32_t hasNormalTexture;
    int32_t hasOcclusionTexture;
    int32_t hasEmissiveTexture;
    int32_t enableLighting;
    int32_t alphaMode; // 0=OPAQUE, 1=MASK, 2=BLEND
    int32_t doubleSided;

    // UV変換
    Matrix4x4 uvTransform;

    // パディングとして追加のフラグを配置
    int32_t enableEnvironmentMap; // 環境マップの有効/無効
    float padding;
};

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Vector3 ambientColor;       // アンビエントライトの色
    float ambientIntensity;     // アンビエントライトの強度
};

struct SpotLight {
    Vector4 color;        // ライトの色（RGBAで、Aは未使用）
    Vector3 position;     // スポットライトの位置
    float intensity;      // ライトの強度
    Vector3 direction;    // スポットライトの方向
    float innerCone;      // 内側コーン角度（cos値）
    Vector3 attenuation;  // 減衰パラメータ（定数、線形、二次）
    float outerCone;      // 外側コーン角度（cos値）
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

// マテリアルデータ構造体の定義
struct MaterialData {
    std::string textureFilePath;  // テクスチャファイルパス
    Vector4 ambient = { 0.1f, 0.1f, 0.1f, 1.0f };  // 環境光(Ka)
    Vector4 diffuse = { 0.8f, 0.8f, 0.8f, 1.0f };  // 拡散反射光(Kd)
    Vector4 specular = { 0.0f, 0.0f, 0.0f, 1.0f }; // 鏡面反射光(Ks)
    float shininess = 0.0f;                      // 光沢度(Ns)
    float alpha = 1.0f;                          // 透明度(d)
    Vector2 textureScale = { 1.0f, 1.0f };       // テクスチャスケール(-s option)
    Vector2 textureOffset = { 0.0f, 0.0f };      // テクスチャオフセット(-o option)
    
    // PBR マテリアルプロパティ
    bool isPBR = false;                          // PBRレンダリングを使用するか
    Vector4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };  // ベースカラー係数
    float metallicFactor = 0.0f;                 // メタリック係数
    float roughnessFactor = 1.0f;                // ラフネス係数
    float normalScale = 1.0f;                    // 法線マップのスケール
    float occlusionStrength = 1.0f;              // オクルージョンの強度
    Vector3 emissiveFactor = { 0.0f, 0.0f, 0.0f }; // エミッシブ係数
    float alphaCutoff = 0.5f;                    // アルファカットオフ
    std::string alphaMode = "OPAQUE";            // アルファモード
    bool doubleSided = false;                    // 両面レンダリング
};

// マテリアルテンプレート構造体（GLTF用）
struct MaterialTemplate {
    float metallic;
    float padding[3];
};

// ボーンウェイト構造体
struct VertexWeightData {
    float weight;
    uint32_t vectorIndex;
};

struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix;
    std::vector<VertexWeightData> vertexWeights;
};

// マテリアルごとの頂点データ（マルチマテリアル対応）
struct MaterialVertexData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    std::map<std::string, JointWeightData> skinClusterData;
    size_t materialIndex;
};

struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    MaterialData material;
    std::map<std::string, JointWeightData> skinClusterData;  // ボーンウェイト情報
    Transform rootTransform;  // ルートノードの変換情報（Blenderで設定されたスケール等）

    // マルチマテリアル対応
    std::map<std::wstring, MaterialVertexData> matVertexData;
    std::vector<MaterialData> materials;
    std::vector<MaterialTemplate> materialTemplates;

    // ノード階層構造（GLTF/アニメーション用）
    // Note: Node構造体はAnimation.hで定義されているため、使用時にインクルード必要
};