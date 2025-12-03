#include "Light.h"
#include "imgui/imgui.h"
#include <cmath>
#include <string>

namespace RC {

Light::Light() {
  // デフォルトは上からの白いライト
  data_.color = {1.0f, 1.0f, 1.0f, 1.0f};
  data_.direction = {0.0f, -1.0f, 0.0f};
  data_.intensity = 1.0f;
}

void Light::SetDirection(const Vector3 &dir, bool normalize) {
  Vector3 d = dir;
  if (normalize) {
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 1e-6f) {
      d.x /= len;
      d.y /= len;
      d.z /= len;
    }
  }
  data_.direction = d;
}

void Light::SetColor(const Vector3 &rgb, float alpha) {
  data_.color = {rgb.x, rgb.y, rgb.z, alpha};
}

void Light::SetColor(const Vector4 &rgba) { data_.color = rgba; }

void Light::SetIntensity(float intensity) { data_.intensity = intensity; }

void Light::DrawImGui(const char *name) {

  std::string label = name ? std::string(name) : std::string("Light");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::TextUnformatted("Lighting");

  static const char *kModes[] = {"None", "Lambert", "HalfLambert", "Phog"};
  int mode = lightingMode_;
  if (ImGui::Combo((std::string("モード##") + label).c_str(), &mode, kModes,
                   IM_ARRAYSIZE(kModes))) {
    lightingMode_ = mode;
  }

  // 光カラー
  ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(), &data_.color.x,
                    ImGuiColorEditFlags_Float);

  // 方向（-1..1）＋正規化
  bool dirChanged =
      ImGui::DragFloat3((std::string("光方向(x,y,z)##") + label).c_str(),
                        &data_.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
  if (dirChanged) {
    Vector3 d = data_.direction;
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 1e-6f) {
      d.x /= len;
      d.y /= len;
      d.z /= len;
      data_.direction = d;
    }
  }

  // 強さ
  ImGui::DragFloat((std::string("強さ##") + label).c_str(), &data_.intensity,
                   0.01f, 0.0f, 16.0f, "%.2f");

  ImGui::Dummy(ImVec2(0, 6));
}

} // namespace RC
