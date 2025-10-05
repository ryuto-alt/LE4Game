#pragma once
#include "Mymath.h"
#include <vector>
#include <map>
#include <string>
#include <optional>
#include <array>
#include <span>
#include <d3d12.h>
#include <wrl.h>

// Quaternionはベクトル4次元として定義
using Quaternion = Vector4;


struct QuaternionTransform {
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};

struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix;
    std::string name;
    std::vector<Node> children;
};

// Vector3のキーフレーム構造体
struct KeyframeVector3 {
    Vector3 value;  // キーフレームの値
    float time;     // キーフレームの時刻（単位は秒）
};

// Quaternionのキーフレーム構造体
struct KeyframeQuaternion {
    Quaternion value;  // キーフレームの値
    float time;        // キーフレームの時刻（単位は秒）
};

// Nodeのアニメーション（translate, rotate, scaleのキーフレーム配列）
struct NodeAnimation {
    std::vector<KeyframeVector3> translate;    // 平行移動のキーフレーム
    std::vector<KeyframeQuaternion> rotate;    // 回転のキーフレーム
    std::vector<KeyframeVector3> scale;        // スケールのキーフレーム
};

// アニメーション全体を表すクラス
struct Animation {
    float duration;  // アニメーション全体の尺（単位は秒）
    std::map<std::string, NodeAnimation> nodeAnimations;  // NodeAnimationの集合。Node名で引けるようにstd::mapで格納
};


struct Joint {
    struct {
        Vector3 scale;
        Quaternion rotate;
        Vector3 translate;
    } transform;                           // Transform情報
    Matrix4x4 localMatrix;                 // localMatrix
    Matrix4x4 skeletonSpaceMatrix;         // skeletonSpaceでの変換行列
    std::string name;                      // 名前
    std::vector<int32_t> children;         // 子JointのIndexのリスト。いなければ空
    int32_t index;                         // 自身のIndex
    std::optional<int32_t> parent;         // 親JointのIndex。いなければnull
};

struct Skeleton {
    int32_t root;                                        // RootJointのIndex
    std::map<std::string, int32_t> jointMap;             // Joint名とIndexとの辞書
    std::vector<Joint> joints;                           // 所属しているジョイント
};


const uint32_t kNumMaxInfluence = 4;
struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights;
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinCluster {
    std::vector<Matrix4x4> inverseBindPoseMatrices;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView;
    std::span<VertexInfluence> mappedInfluence;
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    std::span<WellForGPU> mappedPalette;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
};