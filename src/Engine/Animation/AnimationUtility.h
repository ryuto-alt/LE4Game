#pragma once
#include "Animation.h"
#include "Mymath.h"
#include <string>
#include <vector>

// アニメーション読み込み関数
Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

// 指定した時刻のVector3値を計算（線形補間）
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

// 指定した時刻のQuaternion値を計算（球面線形補間）
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

// 線形補間関数
Vector3 Lerp(const Vector3& start, const Vector3& end, float t);

// 球面線形補間関数
Quaternion Slerp(const Quaternion& start, const Quaternion& end, float t);