#include "Light/Area/AreaLightSource.h"
#include "imgui/imgui.h"
#include <cmath>
#include <string>

namespace RC {

static Vector3 NormalizeSafe_(const Vector3 &v) {
  const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
  if (len2 <= 1e-12f) {
    return {1.0f, 0.0f, 0.0f};
  }
  const float invLen = 1.0f / std::sqrt(len2);
  return {v.x * invLen, v.y * invLen, v.z * invLen};
}

AreaLightSource::AreaLightSource() {
  data_.color = {1, 1, 1, 1};
  data_.position = {0.0f, 2.0f, 0.0f};
  data_.intensity = 1.0f;

  // 既定は X右 / Y上 の板
  data_.right = {1.0f, 0.0f, 0.0f};
  data_.up = {0.0f, 1.0f, 0.0f};
  data_.halfWidth = 0.5f;
  data_.halfHeight = 0.5f;

  data_.range = 10.0f;
  data_.decay = 2.0f;
  data_.twoSided = 0;
}

void AreaLightSource::SetBasis(const Vector3 &right, const Vector3 &up,
                              bool normalize) {
  if (normalize) {
    data_.right = NormalizeSafe_(right);
    data_.up = NormalizeSafe_(up);
  } else {
    data_.right = right;
    data_.up = up;
  }
}

void AreaLightSource::SetSize(float width, float height) {
  data_.halfWidth = width * 0.5f;
  data_.halfHeight = height * 0.5f;
}

void AreaLightSource::DrawImGui(const char *name) {
  std::string label = name ? std::string(name) : std::string("AreaLight");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(), &data_.color.x,
                    ImGuiColorEditFlags_Float);

  ImGui::DragFloat3((std::string("位置##") + label).c_str(), &data_.position.x,
                    0.01f);

  ImGui::DragFloat((std::string("強さ##") + label).c_str(), &data_.intensity,
                   0.01f, 0.0f, 64.0f, "%.2f");

  ImGui::DragFloat3((std::string("right(軸)##") + label).c_str(), &data_.right.x,
                    0.01f);
  ImGui::DragFloat3((std::string("up(軸)##") + label).c_str(), &data_.up.x,
                    0.01f);

  ImGui::DragFloat((std::string("半幅##") + label).c_str(), &data_.halfWidth,
                   0.01f, 0.0f, 1000.0f, "%.2f");
  ImGui::DragFloat((std::string("半高##") + label).c_str(), &data_.halfHeight,
                   0.01f, 0.0f, 1000.0f, "%.2f");

  ImGui::DragFloat((std::string("影響距離(range)##") + label).c_str(),
                   &data_.range, 0.05f, 0.0f, 500.0f, "%.2f");
  ImGui::DragFloat((std::string("減衰(decay)##") + label).c_str(), &data_.decay,
                   0.05f, 0.0f, 32.0f, "%.2f");

  bool two = (data_.twoSided != 0);
  if (ImGui::Checkbox((std::string("両面発光(twoSided)##") + label).c_str(),
                      &two)) {
    data_.twoSided = two ? 1u : 0u;
  }

  ImGui::TextDisabled("※ まずは擬似AreaLight（四隅4サンプル）だよ");
}

} // namespace RC
