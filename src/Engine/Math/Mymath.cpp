#include "Mymath.h"
#include "algorithm"
#include <cassert>
//float Cot(float theta)
//{
//	return 1 / std::tan(theta);
//}

#pragma region 単位行列
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};
	return result;
}
#pragma endregion

#pragma region 4x4Matrix同士の乗算
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}
#pragma endregion

#pragma region 回転行列の作成
Matrix4x4 MakeRotateMatrix(const Vector3& rotate) {
	Matrix4x4 rotateX;
	rotateX = {
	1,0,0,0,
		0,std::cos(rotate.x),std::sin(rotate.x),0,
		0,-std::sin(rotate.x),std::cos(rotate.x),0,
		0,0,0,1
	};
	Matrix4x4 rotateY;
	rotateY = {
	std::cos(rotate.y),0,-std::sin(rotate.y),0,
		0,1,0,0,
		std::sin(rotate.y),0,std::cos(rotate.y),
		0,0,0,0,1
	};
	Matrix4x4 rotateZ;
	rotateZ = {
	std::cos(rotate.z),std::sin(rotate.z),0,0,
		-std::sin(rotate.z),std::cos(rotate.z),0,0,
		0,0,1,0,
		0,0,0,1
	};
	Matrix4x4 result = Multiply(rotateX, Multiply(rotateY, rotateZ));
	return result;
}
#pragma endregion

#pragma region アフィン行列の作成
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 result;
	Matrix4x4 rotateM = MakeRotateMatrix(rotate);
	result = {
		scale.x * rotateM.m[0][0],scale.x * rotateM.m[0][1],scale.x * rotateM.m[0][2],0,
		scale.y * rotateM.m[1][0],scale.y * rotateM.m[1][1],scale.y * rotateM.m[1][2],0,
		scale.z * rotateM.m[2][0],scale.z * rotateM.m[2][1],scale.z * rotateM.m[2][2],0,
		translate.x,translate.y,translate.z,1
	};
	return result;
}
#pragma endregion

#pragma region 逆行列の作成
Matrix4x4 Inverse(const Matrix4x4& m) {
	float determinant =
		+m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]
		+ m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]
		+ m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]

		- m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]
		- m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]
		- m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]

		- m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]
		- m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]
		- m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]

		+ m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]
		+ m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]
		+ m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]

		+ m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]
		+ m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]
		+ m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]

		- m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]
		- m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]
		- m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]

		- m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]
		- m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]
		- m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]

		+ m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]
		+ m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]
		+ m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];

	Matrix4x4 result = {};
	float recpDeterminant = 1.0f / determinant;
	result.m[0][0] = (m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] +
		m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][3] * m.m[2][2] * m.m[3][1] -
		m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]) * recpDeterminant;
	result.m[0][1] = (-m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] -
		m.m[0][3] * m.m[2][1] * m.m[3][2] + m.m[0][3] * m.m[2][2] * m.m[3][1] +
		m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2]) * recpDeterminant;
	result.m[0][2] = (m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] +
		m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[3][1] -
		m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]) * recpDeterminant;
	result.m[0][3] = (-m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] -
		m.m[0][3] * m.m[1][1] * m.m[2][2] + m.m[0][3] * m.m[1][2] * m.m[2][1] +
		m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2]) * recpDeterminant;

	result.m[1][0] = (-m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] -
		m.m[1][3] * m.m[2][0] * m.m[3][2] + m.m[1][3] * m.m[2][2] * m.m[3][0] +
		m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2]) * recpDeterminant;
	result.m[1][1] = (m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] +
		m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[2][2] * m.m[3][0] -
		m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]) * recpDeterminant;
	result.m[1][2] = (-m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] -
		m.m[0][3] * m.m[1][0] * m.m[3][2] + m.m[0][3] * m.m[1][2] * m.m[3][0] +
		m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2]) * recpDeterminant;
	result.m[1][3] = (m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] +
		m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][3] * m.m[1][2] * m.m[2][0] -
		m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]) * recpDeterminant;

	result.m[2][0] = (m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] +
		m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][3] * m.m[2][1] * m.m[3][0] -
		m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]) * recpDeterminant;
	result.m[2][1] = (-m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] -
		m.m[0][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[2][1] * m.m[3][0] +
		m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1]) * recpDeterminant;
	result.m[2][2] = (m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] +
		m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][3] * m.m[1][1] * m.m[3][0] -
		m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]) * recpDeterminant;
	result.m[2][3] = (-m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] -
		m.m[0][3] * m.m[1][0] * m.m[2][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] +
		m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1]) * recpDeterminant;

	result.m[3][0] = (-m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] -
		m.m[1][2] * m.m[2][0] * m.m[3][1] + m.m[1][2] * m.m[2][1] * m.m[3][0] +
		m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1]) * recpDeterminant;
	result.m[3][1] = (m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] +
		m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[2][1] * m.m[3][0] -
		m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]) * recpDeterminant;
	result.m[3][2] = (-m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] -
		m.m[0][2] * m.m[1][0] * m.m[3][1] + m.m[0][2] * m.m[1][1] * m.m[3][0] +
		m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1]) * recpDeterminant;
	result.m[3][3] = (m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] +
		m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] -
		m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1]) * recpDeterminant;

	return result;
}
#pragma endregion

#pragma region コタンジェント
//float cot(float x) {
//	float cot;
//	cot = 1 / std::tan(x);
//	return cot;
//}
#pragma endregion

#pragma region 透視投影行列
//Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
//	Matrix4x4 result;
//	result = {
//		(1 / aspectRatio) * cot(fovY / 2),0,0,0,
//		0,cot(fovY / 2),0,0,
//		0,0,farClip / (farClip - nearClip),1,
//		0,0,(-nearClip * farClip) / (farClip - nearClip),0
//	};
//	return result;
//}
#pragma endregion

#pragma region 平行投影行列
//Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
//
//	Matrix4x4 result;
//
//	result = {
//	2 / (right - left),0.0f,0.0f,0.0f,
//	0.0f,2 / (top - bottom),0.0f,0.0f,
//	0.0f,0.0f,1 / (farClip - nearClip),0.0f,
//	(left + right) / (left - right),(top + bottom) / (bottom - top),nearClip / (nearClip - farClip),1.0f
//	};
//
//	return result;
//}
#pragma endregion

#pragma region ビューポート
//Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth)
//{
//	Matrix4x4 ans;
//
//
//
//	ans.m[0][0] = width / 2;
//	ans.m[0][1] = 0;
//	ans.m[0][2] = 0;
//	ans.m[0][3] = 0;
//
//	ans.m[1][0] = 0;
//	ans.m[1][1] = -(height / 2);
//	ans.m[1][2] = 0;
//	ans.m[1][3] = 0;
//
//	ans.m[2][0] = 0;
//	ans.m[2][1] = 0;
//	ans.m[2][2] = maxDepth - minDepth;
//	ans.m[2][3] = 0;
//
//	ans.m[3][0] = left + (width / 2);
//	ans.m[3][1] = top + (height / 2);
//	ans.m[3][2] = minDepth;
//	ans.m[3][3] = 1;
//
//
//
//
//	return ans;
//
//}
#pragma endregion

Matrix4x4 MakeScaleMatrix(const Vector3& scale)
{
	Matrix4x4 ans;

	ans.m[0][0] = scale.x;
	ans.m[0][1] = 0;
	ans.m[0][2] = 0;
	ans.m[0][3] = 0;

	ans.m[1][0] = 0;
	ans.m[1][1] = scale.y;
	ans.m[1][2] = 0;
	ans.m[1][3] = 0;

	ans.m[2][0] = 0;
	ans.m[2][1] = 0;
	ans.m[2][2] = scale.z;
	ans.m[2][3] = 0;

	ans.m[3][0] = 0;
	ans.m[3][1] = 0;
	ans.m[3][2] = 0;
	ans.m[3][3] = 1;
	return ans;
}

Matrix4x4 MakeRotateXMatrix(float radian)
{
	Matrix4x4 ans;

	ans.m[0][0] = 1;
	ans.m[0][1] = 0;
	ans.m[0][2] = 0;
	ans.m[0][3] = 0;

	ans.m[1][0] = 0;
	ans.m[1][1] = std::cos(radian);;
	ans.m[1][2] = std::sin(radian);;
	ans.m[1][3] = 0;

	ans.m[2][0] = 0;
	ans.m[2][1] = -std::sin(radian);;
	ans.m[2][2] = std::cos(radian);;
	ans.m[2][3] = 0;

	ans.m[3][0] = 0;
	ans.m[3][1] = 0;
	ans.m[3][2] = 0;
	ans.m[3][3] = 1;

	return ans;
}

Matrix4x4 MakeRotateYMatrix(float radian)
{
	Matrix4x4 ans;

	ans.m[0][0] = std::cos(radian);
	ans.m[0][1] = 0;
	ans.m[0][2] = -std::sin(radian);
	ans.m[0][3] = 0;

	ans.m[1][0] = 0;
	ans.m[1][1] = 1;
	ans.m[1][2] = 0;
	ans.m[1][3] = 0;

	ans.m[2][0] = std::sin(radian);;
	ans.m[2][1] = 0;
	ans.m[2][2] = std::cos(radian);;
	ans.m[2][3] = 0;

	ans.m[3][0] = 0;
	ans.m[3][1] = 0;
	ans.m[3][2] = 0;
	ans.m[3][3] = 1;

	return ans;
}

Matrix4x4 MakeRotateZMatrix(float radian)
{
	Matrix4x4 ans;

	ans.m[0][0] = std::cos(radian);
	ans.m[0][1] = std::sin(radian);
	ans.m[0][2] = 0;
	ans.m[0][3] = 0;

	ans.m[1][0] = -std::sin(radian);
	ans.m[1][1] = std::cos(radian);
	ans.m[1][2] = 0;
	ans.m[1][3] = 0;

	ans.m[2][0] = 0;
	ans.m[2][1] = 0;
	ans.m[2][2] = 1;
	ans.m[2][3] = 0;

	ans.m[3][0] = 0;
	ans.m[3][1] = 0;
	ans.m[3][2] = 0;
	ans.m[3][3] = 1;


	return ans;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate)
{
	Matrix4x4 ans;

	ans.m[0][0] = 1;
	ans.m[0][1] = 0;
	ans.m[0][2] = 0;
	ans.m[0][3] = 0;

	ans.m[1][0] = 0;
	ans.m[1][1] = 1;
	ans.m[1][2] = 0;
	ans.m[1][3] = 0;

	ans.m[2][0] = 0;
	ans.m[2][1] = 0;
	ans.m[2][2] = 1;
	ans.m[2][3] = 0;

	ans.m[3][0] = translate.x;
	ans.m[3][1] = translate.y;
	ans.m[3][2] = translate.z;
	ans.m[3][3] = 1;

	return ans;
}

Matrix4x4 Transpose(const Matrix4x4& m)
{
    Matrix4x4 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = m.m[j][i];
        }
    }
    return result;
}

Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t)
{
    // 入力値のNaNチェック
    assert(!std::isnan(v1.x) && !std::isnan(v1.y) && !std::isnan(v1.z));
    assert(!std::isnan(v2.x) && !std::isnan(v2.y) && !std::isnan(v2.z));
    assert(!std::isnan(t));
    
    // tが0.0から1.0の範囲内にあることを確認
    assert(t >= 0.0f && t <= 1.0f);
    
    Vector3 result;
    result.x = v1.x + t * (v2.x - v1.x);
    result.y = v1.y + t * (v2.y - v1.y);
    result.z = v1.z + t * (v2.z - v1.z);

    // 結果のNaNチェック
    assert(!std::isnan(result.x));
    assert(!std::isnan(result.y));
    assert(!std::isnan(result.z));
    return result;
}

float Dot(const Vector4& v1, const Vector4& v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

Vector4 Normalize(const Vector4& v)
{
	float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
	if (length == 0.0f) {
		return v;
	}
	return { v.x / length, v.y / length, v.z / length, v.w / length };
}

Vector4 Slerp(const Vector4& befor, const Vector4& after, float t)
{
	// 入力値のNaNチェック
	assert(!std::isnan(befor.x) && !std::isnan(befor.y) && !std::isnan(befor.z) && !std::isnan(befor.w));
	assert(!std::isnan(after.x) && !std::isnan(after.y) && !std::isnan(after.z) && !std::isnan(after.w));
	assert(!std::isnan(t));
	
	// tが0.0から1.0の範囲内にあることを確認
	assert(t >= 0.0f && t <= 1.0f);
	
	Vector4 quat0 = befor;
	Vector4 quat1 = after;
	Vector4 result;

	float dot = Dot(quat0, quat1);
	assert(!std::isnan(dot));
	
	// 最短経路で補間するため、dotが負の場合はクォータニオンを反転
	if (dot < 0.0f)
	{
		quat1.x = -quat1.x;
		quat1.y = -quat1.y;
		quat1.z = -quat1.z;
		quat1.w = -quat1.w;
		dot = -dot;
	}

	dot = std::clamp(dot, -1.0f, 1.0f);
	const float epsilon = 1e-6f;

	if (dot > 1.0f - epsilon)
	{
		result = Normalize((1.0f - t) * quat0 + t * quat1);
	}
	else
	{
		// なす角を求める
		float theta = std::acos(dot);
		assert(!std::isnan(theta));
		
		// ゼロ除算チェック
		assert(std::sin(theta) != 0.0f);
		
		float scale0 = std::sin((1.0f - t) * theta) / std::sin(theta);
		float scale1 = std::sin(t * theta) / std::sin(theta);
		
		assert(!std::isnan(scale0));
		assert(!std::isnan(scale1));

		result = Normalize(scale0 * quat0 + scale1 * quat1);
	}

	// 結果のNaNチェック
	assert(!std::isnan(result.x));
	assert(!std::isnan(result.y));
	assert(!std::isnan(result.z));
	assert(!std::isnan(result.w));
	return result;
}


Matrix4x4 MakeRotateMatrix(const Vector4& rotate)
{
    // 入力値のNaNチェック
    assert(!std::isnan(rotate.x) && !std::isnan(rotate.y) && !std::isnan(rotate.z) && !std::isnan(rotate.w));
    
    // クォータニオンの長さをチェック（正規化されているべき）
    float lengthSq = rotate.x * rotate.x + rotate.y * rotate.y + rotate.z * rotate.z + rotate.w * rotate.w;
    assert(!std::isnan(lengthSq));
    assert(std::abs(lengthSq - 1.0f) < 0.1f); // 正規化されているかチェック（許容誤差を緩める）

    float x2 = rotate.x + rotate.x;
    float y2 = rotate.y + rotate.y;
    float z2 = rotate.z + rotate.z;
    float xx = rotate.x * x2;
    float xy = rotate.x * y2;
    float xz = rotate.x * z2;
    float yy = rotate.y * y2;
    float yz = rotate.y * z2;
    float zz = rotate.z * z2;
    float wx = rotate.w * x2;
    float wy = rotate.w * y2;
    float wz = rotate.w * z2;
    
    Matrix4x4 result;
    result.m[0][0] = 1.0f - (yy + zz);
    result.m[0][1] = xy + wz;
    result.m[0][2] = xz - wy;
    result.m[0][3] = 0.0f;
    
    result.m[1][0] = xy - wz;
    result.m[1][1] = 1.0f - (xx + zz);
    result.m[1][2] = yz + wx;
    result.m[1][3] = 0.0f;
    
    result.m[2][0] = xz + wy;
    result.m[2][1] = yz - wx;
    result.m[2][2] = 1.0f - (xx + yy);
    result.m[2][3] = 0.0f;
    
    result.m[3][0] = 0.0f;
    result.m[3][1] = 0.0f;
    result.m[3][2] = 0.0f;
    result.m[3][3] = 1.0f;
    
    // 結果の行列の各要素がNaNでないことを確認
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            assert(!std::isnan(result.m[i][j]));
        }
    }
    
    return result;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector4& rotate, const Vector3& translate)
{
    // 入力値のNaNチェック
    assert(!std::isnan(scale.x) && !std::isnan(scale.y) && !std::isnan(scale.z));
    assert(!std::isnan(rotate.x) && !std::isnan(rotate.y) && !std::isnan(rotate.z) && !std::isnan(rotate.w));
    assert(!std::isnan(translate.x) && !std::isnan(translate.y) && !std::isnan(translate.z));
    
    Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
    
    Matrix4x4 result = Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    
    // 結果の行列の各要素がNaNでないことを確認
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            assert(!std::isnan(result.m[i][j]));
        }
    }
    
    return result;
}