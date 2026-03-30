#include "PointLightSource.h"
#include "imgui/imgui.h"
#include <string>

namespace RC {

PointLightSource::PointLightSource() {
  data_.color = {1, 1, 1, 1};
  data_.position = {0.0f, 2.0f, 0.0f};
  data_.intensity = 1.0f;
  data_.radius = 10.0f;
  data_.decay = 2.0f;
}

::PointLight PointLightSource::DataForGPU() const {
  ::PointLight out = data_;
  if (!enabled_) {
    out.intensity = 0.0f;
  }
  return out;
}

void PointLightSource::DrawImGui(const char *name) {
#if RC_ENABLE_IMGUI

  std::string label = name ? std::string(name) : std::string("PointLight");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::Checkbox((std::string("有効##") + label).c_str(), &enabled_);

  if (enabled_) {
    ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(),
                      &data_.color.x, ImGuiColorEditFlags_Float);

    ImGui::DragFloat3((std::string("位置##") + label).c_str(),
                      &data_.position.x, 0.01f);

    ImGui::DragFloat((std::string("強さ##") + label).c_str(), &data_.intensity,
                     0.01f, 0.0f, 64.0f, "%.2f");

    ImGui::DragFloat((std::string("半径(radius)##") + label).c_str(),
                     &data_.radius, 0.05f, 0.0f, 500.0f, "%.2f");

    ImGui::DragFloat((std::string("減衰(decay)##") + label).c_str(),
                     &data_.decay, 0.05f, 0.0f, 32.0f, "%.2f");
  }

#endif
}

} // namespace RC
