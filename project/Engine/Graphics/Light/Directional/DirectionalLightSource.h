#pragma once
#include "Math/MathTypes.h" // DirectionalLight, Vector3, Vector4 など
#include "struct.h"

namespace RC {

// シンプルな方向性ライトクラス
class DirectionalLightSource {
public:
  DirectionalLightSource();

  // DirectionalLight 生データにアクセス
  DirectionalLight &Data() { return data_; }
  const DirectionalLight &Data() const { return data_; }

  // ON/OFF
  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  // GPU に送る実効データ（無効時は intensity=0）
  DirectionalLight DataForGPU() const;

  // 便利 setter
  void SetDirection(const Vector3 &dir, bool normalize = true);
  void SetColor(const Vector3 &rgb, float alpha = 1.0f); // RGB + A
  void SetColor(const Vector4 &rgba);                    // RGBA そのまま
  void SetIntensity(float intensity);

  // ライティングモード（0:None,1:Lambert,2:HalfLambert）
  int GetLightingMode() const { return lightingMode_; }
  void SetLightingMode(int m) { lightingMode_ = m; }

  void SetShininess(float s) { shininess_ = s; }
  float GetShininess() const { return shininess_; }

  void DrawImGui(const char *name = nullptr);

private:
  DirectionalLight data_{};
  bool enabled_ = true;
  int lightingMode_ = 2; // デフォルト: HalfLambert
  float shininess_ = 32.0f;
};

} // namespace RC
