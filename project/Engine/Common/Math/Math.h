#pragma once
#include "MathTypes.h"
#include "struct.h"

//==================================
// RC::Vector3 関連関数
//==================================

// 加算
RC::Vector3 Add(const RC::Vector3 &v1, const RC::Vector3 &v2);
// 減算
RC::Vector3 Subtract(const RC::Vector3 &v1, const RC::Vector3 &v2);
// スカラー倍
RC::Vector3 Multiply(const RC::Vector3 &v1, float scalar);
// 内積
float Dot(const RC::Vector3 &v1, const RC::Vector3 &v2);
// 長さ
float Length(const RC::Vector3 &v);
// 正規化
RC::Vector3 Normalize(const RC::Vector3 &v);
// 座標変換
RC::Vector3 Vector3Transform(const RC::Vector3 &vector, const RC::Matrix4x4 &matrix);
// 正射影ベクトル
RC::Vector3 project(const RC::Vector3 &v1, const RC::Vector3 &v2);
// 最近接点
RC::Vector3 closestPoint(const RC::Vector3 &point, const Segment& segment);
// 直線の交点
RC::Vector3 Cross(const RC::Vector3 &v1, const RC::Vector3 &v2);

//==================================
// RC::Matrix4x4 関連関数
//==================================

// 行列の加法
RC::Matrix4x4 Add(const RC::Matrix4x4 &m1, const RC::Matrix4x4 &m2);
// 行列の減法
RC::Matrix4x4 Subtract(const RC::Matrix4x4 &m1, const RC::Matrix4x4 &m2);
// 行列の積
RC::Matrix4x4 Multiply(const RC::Matrix4x4 &m1, const RC::Matrix4x4 &m2);
// 逆行列
RC::Matrix4x4 Inverse(const RC::Matrix4x4 &m);
// 転置行列
RC::Matrix4x4 Transpose(const RC::Matrix4x4 &m);
// 単位行列の生成
RC::Matrix4x4 MakeIdentity4x4();

//==================================
// 回転行列
//==================================

RC::Matrix4x4 MakeRotateMatrix(RC::ShaftType shaft, float radian);

//==================================
// 平行移動行列
//==================================

RC::Matrix4x4 MakeTranslateMatrix(const RC::Vector3 &translate);

//==================================
// 拡大縮小行列
//==================================

RC::Matrix4x4 MakeScaleMatrix(const RC::Vector3 &scale);

//==================================
// Affine関数
//==================================
RC::Matrix4x4 MakeAffineMatrix(const RC::Vector3 &scale, const RC::Vector3 &rotate,
                           const RC::Vector3 &translate);

//==================================
// 透視投影行列
//==================================

RC::Matrix4x4 MakePerspectiveFovMatrix(float fov, float aspectRatio, float nearClip,
                                   float farClip);

//==================================
// 正射影行列
//==================================

RC::Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
                                 float bottom, float nearClip, float farClip);

//==================================
// ビューポート変換行列
//==================================

RC::Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height,
                             float minDepth, float maxDepth);
