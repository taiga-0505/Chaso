#pragma once
#include "struct.h"
#include <vector>

namespace RC {

class MeshGenerator {
public:
  /// <summary>
  /// 平面（XZ平面）を生成
  /// </summary>
  static ModelData GeneratePlane(float width = 1.0f, float height = 1.0f,
                                 uint32_t segmentsW = 1, uint32_t segmentsH = 1);

  /// <summary>
  /// 直方体を生成
  /// </summary>
  static ModelData GenerateBox(float width = 1.0f, float height = 1.0f,
                               float depth = 1.0f);

  /// <summary>
  /// 円（XZ平面）を生成
  /// </summary>
  static ModelData GenerateCircle(float radius = 1.0f, uint32_t segments = 32);

  /// <summary>
  /// リング（XZ平面、ドーナツ状の円板）を生成
  /// </summary>
  static ModelData GenerateRing(float innerRadius = 0.5f,
                                float outerRadius = 1.0f,
                                uint32_t segments = 32);

  /// <summary>
  /// 拡張リング生成（角度指定、カラー、UV方向などの詳細設定対応）
  /// </summary>
  static ModelData GenerateRingEx(float innerRadius = 0.5f,
                                  float outerRadius = 1.0f,
                                  uint32_t segments = 32,
                                  const Vector4 &innerColor = {1, 1, 1, 1},
                                  const Vector4 &outerColor = {1, 1, 1, 1},
                                  float startAngle = 0.0f,
                                  float endAngle = 360.0f, // 度数法で指定
                                  bool isVerticalUV = false,
                                  bool isXY = false); // デフォルトXZ, trueでXY

  /// <summary>
  /// 球体を生成（UV階層）
  /// </summary>
  static ModelData GenerateSphere(float radius = 1.0f, uint32_t slices = 32,
                                  uint32_t stacks = 16);

  /// <summary>
  /// 円柱を生成
  /// </summary>
  static ModelData GenerateCylinder(float radius = 0.5f, float height = 1.0f,
                                    uint32_t segments = 32);

  /// <summary>
  /// 円錐を生成
  /// </summary>
  static ModelData GenerateCone(float radius = 0.5f, float height = 1.0f,
                                uint32_t segments = 32);

  /// <summary>
  /// トーラスを生成
  /// </summary>
  static ModelData GenerateTorus(float majorRadius = 1.0f,
                                 float minorRadius = 0.2f,
                                 uint32_t majorSegments = 32,
                                 uint32_t minorSegments = 16);

  /// <summary>
  /// カプセルを生成
  /// </summary>
  static ModelData GenerateCapsule(float radius = 0.5f, float height = 2.0f,
                                   uint32_t slices = 32, uint32_t stacks = 16);

  /// <summary>
  /// 単一の三角形を生成
  /// </summary>
  static ModelData GenerateTriangle(const Vector3 &v0, const Vector3 &v1,
                                    const Vector3 &v2);
};

} // namespace RC
