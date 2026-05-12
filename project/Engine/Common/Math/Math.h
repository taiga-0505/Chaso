#pragma once
#include "MathTypes.h"
#include "struct.h"

//==================================
// RC::Vector3 関連関数
//==================================

/// @brief 3次元ベクトルの加算
/// @param v1 ベクトル1
/// @param v2 ベクトル2
/// @return 加算結果
RC::Vector3 Add(const RC::Vector3 &v1, const RC::Vector3 &v2);

/// @brief 3次元ベクトルの減算
/// @param v1 ベクトル1
/// @param v2 ベクトル2
/// @return 減算結果
RC::Vector3 Subtract(const RC::Vector3 &v1, const RC::Vector3 &v2);

/// @brief 3次元ベクトルのスカラー倍
/// @param v1 ベクトル
/// @param scalar 倍率
/// @return 乗算結果
RC::Vector3 Multiply(const RC::Vector3 &v1, float scalar);

/// @brief 3次元ベクトルの内積
/// @param v1 ベクトル1
/// @param v2 ベクトル2
/// @return 内積結果
float Dot(const RC::Vector3 &v1, const RC::Vector3 &v2);

/// @brief 3次元ベクトルの長さを計算
/// @param v ベクトル
/// @return 長さ
float Length(const RC::Vector3 &v);

/// @brief 3次元ベクトルの正規化
/// @param v ベクトル
/// @return 正規化されたベクトル
RC::Vector3 Normalize(const RC::Vector3 &v);

/// @brief ベクトルを行列で座標変換
/// @param vector ベクトル
/// @param matrix 変換行列
/// @return 変換後のベクトル
RC::Vector3 Vector3Transform(const RC::Vector3 &vector, const RC::Matrix4x4 &matrix);

/// @brief 正射影ベクトルの計算
/// @param v1 対象ベクトル
/// @param v2 投影先ベクトル
/// @return 投影後のベクトル
RC::Vector3 project(const RC::Vector3 &v1, const RC::Vector3 &v2);

/// @brief 点から線分への最近接点を求める
/// @param point 座標
/// @param segment 線分
/// @return 線分上の最近接点
RC::Vector3 closestPoint(const RC::Vector3 &point, const Segment& segment);

/// @brief 3次元ベクトルの外積
/// @param v1 ベクトル1
/// @param v2 ベクトル2
/// @return 外積結果
RC::Vector3 Cross(const RC::Vector3 &v1, const RC::Vector3 &v2);

//==================================
// RC::Matrix4x4 関連関数
//==================================

/// @brief 4x4行列の加算
/// @param m1 行列1
/// @param m2 行列2
/// @return 加算結果
RC::Matrix4x4 Add(const RC::Matrix4x4 &m1, const RC::Matrix4x4 &m2);

/// @brief 4x4行列の減算
/// @param m1 行列1
/// @param m2 行列2
/// @return 減算結果
RC::Matrix4x4 Subtract(const RC::Matrix4x4 &m1, const RC::Matrix4x4 &m2);

/// @brief 4x4行列の積
/// @param m1 行列1
/// @param m2 行列2
/// @return 乗算結果
RC::Matrix4x4 Multiply(const RC::Matrix4x4 &m1, const RC::Matrix4x4 &m2);

/// @brief 4x4行列の逆行列を計算
/// @param m 行列
/// @return 逆行列
RC::Matrix4x4 Inverse(const RC::Matrix4x4 &m);

/// @brief 4x4行列の転置行列を計算
/// @param m 行列
/// @return 転置行列
RC::Matrix4x4 Transpose(const RC::Matrix4x4 &m);

/// @brief 4x4単位行列の生成
/// @return 単位行列
RC::Matrix4x4 MakeIdentity4x4();

//==================================
// 回転行列
//==================================

/// @brief 指定した軸の回転行列を生成
/// @param shaft 回転軸 (X, Y, Z)
/// @param radian 回転角 (ラジアン)
/// @return 回転行列
RC::Matrix4x4 MakeRotateMatrix(RC::ShaftType shaft, float radian);

//==================================
// 平行移動行列
//==================================

/// @brief 平行移動行列を生成
/// @param translate 移動量
/// @return 平行移動行列
RC::Matrix4x4 MakeTranslateMatrix(const RC::Vector3 &translate);

//==================================
// 拡大縮小行列
//==================================

/// @brief 拡大縮小行列を生成
/// @param scale 各軸の倍率
/// @return 拡大縮小行列
RC::Matrix4x4 MakeScaleMatrix(const RC::Vector3 &scale);

//==================================
// Affine関数
//==================================

/// @brief アフィン変換行列を生成 (Scale -> Rotate -> Translate)
/// @param scale 拡大率
/// @param rotate 回転角 (x, y, z)
/// @param translate 平行移動
/// @return アフィン変換行列
RC::Matrix4x4 MakeAffineMatrix(const RC::Vector3 &scale, const RC::Vector3 &rotate,
                           const RC::Vector3 &translate);

//==================================
// 透視投影行列
//==================================

/// @brief 透視投影行列を生成
/// @param fov 垂直画角 (ラジアン)
/// @param aspectRatio アスペクト比
/// @param nearClip ニアクリップ距離
/// @param farClip ファークリップ距離
/// @return 透視投影行列
RC::Matrix4x4 MakePerspectiveFovMatrix(float fov, float aspectRatio, float nearClip,
                                   float farClip);

//==================================
// 正射影行列
//==================================

/// @brief 正射影行列を生成
/// @param left 左端
/// @param top 上端
/// @param right 右端
/// @param bottom 下端
/// @param nearClip ニアクリップ
/// @param farClip ファークリップ
/// @return 正射影行列
RC::Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
                                 float bottom, float nearClip, float farClip);

//==================================
// ビューポート変換行列
//==================================

/// @brief ビューポート変換行列を生成
/// @param left ビューポート左上X
/// @param top ビューポート左上Y
/// @param width 幅
/// @param height 高さ
/// @param minDepth 最小深度
/// @param maxDepth 最大深度
/// @return ビューポート変換行列
RC::Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height,
                             float minDepth, float maxDepth);
