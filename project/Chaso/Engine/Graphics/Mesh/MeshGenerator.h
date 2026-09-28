#pragma once
#include "struct.h"
#include <vector>

namespace RC {

/// @brief 基本的なプリミティブ形状のメッシュデータを生成する静的クラス
class MeshGenerator {
public:
  /// @brief 平面メッシュ（XZ平面）を生成
  /// @param width 幅
  /// @param height 奥行き
  /// @param segmentsW 横方向の分割数
  /// @param segmentsH 縦方向の分割数
  /// @return 生成されたメッシュデータ
  static ModelData GeneratePlane(float width = 1.0f, float height = 1.0f,
                                 uint32_t segmentsW = 1, uint32_t segmentsH = 1);

  /// @brief 直方体メッシュを生成
  /// @param width 幅
  /// @param height 高さ
  /// @param depth 奥行き
  /// @return 生成されたメッシュデータ
  static ModelData GenerateBox(float width = 1.0f, float height = 1.0f,
                               float depth = 1.0f);

  /// @brief 円盤メッシュ（XZ平面）を生成
  /// @param radius 半径
  /// @param segments 円周の分割数
  /// @return 生成されたメッシュデータ
  static ModelData GenerateCircle(float radius = 1.0f, uint32_t segments = 32);

  /// @brief リング状（ドーナツ状）の円板メッシュ（XZ平面）を生成
  /// @param innerRadius 内半径
  /// @param outerRadius 外半径
  /// @param segments 円周の分割数
  /// @return 生成されたメッシュデータ
  static ModelData GenerateRing(float innerRadius = 0.5f,
                                float outerRadius = 1.0f,
                                uint32_t segments = 32);

  /// @brief 拡張リングメッシュの生成
  /// 角度指定やカラー、UVの方向設定などが可能な詳細版です。
  /// @param innerRadius 内半径
  /// @param outerRadius 外半径
  /// @param segments 円周の分割数
  /// @param innerColor 内側の頂点カラー
  /// @param outerColor 外側の頂点カラー
  /// @param startAngle 開始角度（度）
  /// @param endAngle 終了角度（度）
  /// @param isVerticalUV UVを縦方向に生成するか
  /// @param isXY trueならXY平面、falseならXZ平面に生成
  /// @return 生成されたメッシュデータ
  static ModelData GenerateRingEx(float innerRadius = 0.5f,
                                  float outerRadius = 1.0f,
                                  uint32_t segments = 32,
                                  const Vector4 &innerColor = {1, 1, 1, 1},
                                  const Vector4 &outerColor = {1, 1, 1, 1},
                                  float startAngle = 0.0f,
                                  float endAngle = 360.0f,
                                  bool isVerticalUV = false,
                                  bool isXY = false);

  /// @brief 球体メッシュを生成
  /// @param radius 半径
  /// @param slices 経度方向の分割数（縦）
  /// @param stacks 緯度方向の分割数（横）
  /// @return 生成されたメッシュデータ
  static ModelData GenerateSphere(float radius = 1.0f, uint32_t slices = 32,
                                  uint32_t stacks = 16);

  /// @brief 円柱メッシュを生成
  /// @param radius 半径
  /// @param height 高さ
  /// @param segments 円周의 分割数
  /// @return 生成されたメッシュデータ
  static ModelData GenerateCylinder(float radius = 0.5f, float height = 1.0f,
                                    uint32_t segments = 32);

  /// @brief エフェクト用円柱メッシュを生成
  /// 上下の蓋がなく、UV展開や傾斜（上下半径の差）などの調整が可能な、エフェクト向けの生成メソッドです。
  /// @param topRadius 上面の半径
  /// @param bottomRadius 底面の半径
  /// @param height 高さ
  /// @param segments 円周の分割数
  /// @param startAngle 開始角度（度）
  /// @param endAngle 終了角度（度）
  /// @param isVerticalUV UVを縦方向に生成するか
  /// @param flipV V座標を反転させるか
  /// @return 生成されたメッシュデータ
  static ModelData GenerateEffectCylinder(
      float topRadius = 1.0f, float bottomRadius = 1.0f, float height = 3.0f,
      uint32_t segments = 32, float startAngle = 0.0f, float endAngle = 360.0f,
      bool isVerticalUV = true, bool flipV = false);

  /// @brief 円錐メッシュを生成
  /// @param radius 底面の半径
  /// @param height 高さ
  /// @param segments 底面の分割数
  /// @return 生成されたメッシュデータ
  static ModelData GenerateCone(float radius = 0.5f, float height = 1.0f,
                                uint32_t segments = 32);

  /// @brief トーラス（円環）メッシュを生成
  /// @param majorRadius 主半径（中心から管の中心まで）
  /// @param minorRadius 副半径（管自体の半径）
  /// @param majorSegments 主半径方向の分割数
  /// @param minorSegments 副半径方向の分割数
  /// @return 生成されたメッシュデータ
  static ModelData GenerateTorus(float majorRadius = 1.0f,
                                 float minorRadius = 0.2f,
                                 uint32_t majorSegments = 32,
                                 uint32_t minorSegments = 16);

  /// @brief カプセルメッシュを生成
  /// @param radius 半径
  /// @param height 高さ（球体部分を含む全高）
  /// @param slices 経度方向の分割数
  /// @param stacks 緯度方向の分割数
  /// @return 生成されたメッシュデータ
  static ModelData GenerateCapsule(float radius = 0.5f, float height = 2.0f,
                                   uint32_t slices = 32, uint32_t stacks = 16);

  /// @brief 単一の三角形メッシュを生成
  /// @param v0 第1頂点
  /// @param v1 第2頂点
  /// @param v2 第3頂点
  /// @return 生成されたメッシュデータ
  static ModelData GenerateTriangle(const Vector3 &v0, const Vector3 &v1,
                                    const Vector3 &v2);
};

} // namespace RC
