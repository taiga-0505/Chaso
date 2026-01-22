#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

/// <summary>
/// Rect AreaLight の操作用クラス（CPU側）
/// - 実体は struct AreaLight（GPUへ送るデータ）
/// - right / up は「面の向き」を表す軸（正規化推奨）
/// - halfWidth / halfHeight は半サイズ
/// - range は影響距離（0以下なら無効）
/// </summary>
class AreaLightSource {
public:
  AreaLightSource();

  ::AreaLight &Data() { return data_; }
  const ::AreaLight &Data() const { return data_; }

  void SetPosition(const Vector3 &pos) { data_.position = pos; }

  void SetColor(const Vector3 &rgb, float alpha = 1.0f) {
    data_.color = {rgb.x, rgb.y, rgb.z, alpha};
  }
  void SetColor(const Vector4 &rgba) { data_.color = rgba; }

  void SetIntensity(float v) { data_.intensity = v; }

  /// right/up の基底（必要なら正規化）
  void SetBasis(const Vector3 &right, const Vector3 &up, bool normalize = true);

  /// 幅/高さ（フルサイズ指定）
  void SetSize(float width, float height);

  void SetHalfSize(float halfWidth, float halfHeight) {
    data_.halfWidth = halfWidth;
    data_.halfHeight = halfHeight;
  }

  void SetRange(float r) { data_.range = r; }
  void SetDecay(float d) { data_.decay = d; }

  void SetTwoSided(bool b) { data_.twoSided = b ? 1u : 0u; }
  bool IsTwoSided() const { return data_.twoSided != 0; }

  void DrawImGui(const char *name = nullptr);

private:
  ::AreaLight data_{};
};

} // namespace RC
