#include "SpotLightSource.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace RC {

static float DegToRad_(float deg) { return deg * 3.1415926535f / 180.0f; }
static float RadToDeg_(float rad) { return rad * 180.0f / 3.1415926535f; }

SpotLightSource::SpotLightSource() {
  data_.color = {1, 1, 1, 1};
  data_.position = {0.0f, 2.0f, 0.0f};
  data_.intensity = 1.0f;

  // デフォルトは「前方 -Z 方向」に照らす想定（必要なら好きに）
  data_.direction = {0.0f, -1.0f, 0.0f};

  data_.distance = 15.0f;
  data_.decay = 2.0f;

  // 30度くらいのカットオフ
  data_.cosAngle = std::cos(DegToRad_(30.0f));
}

void SpotLightSource::SetAngleDeg(float deg) {
  deg = std::clamp(deg, 0.0f, 89.9f);
  data_.cosAngle = std::cos(DegToRad_(deg));
}

void SpotLightSource::SetAngleRad(float rad) {
  rad = std::clamp(rad, 0.0f, 3.1415926535f * 0.499f);
  data_.cosAngle = std::cos(rad);
}

void SpotLightSource::DrawImGui(const char *name) {
  std::string label = name ? std::string(name) : std::string("SpotLight");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(), &data_.color.x,
                    ImGuiColorEditFlags_Float);

  ImGui::DragFloat3((std::string("位置##") + label).c_str(), &data_.position.x,
                    0.01f);

  ImGui::DragFloat3((std::string("方向##") + label).c_str(), &data_.direction.x,
                    0.01f);

  ImGui::DragFloat((std::string("強さ##") + label).c_str(), &data_.intensity,
                   0.01f, 0.0f, 64.0f, "%.2f");

  ImGui::DragFloat((std::string("距離(distance)##") + label).c_str(),
                   &data_.distance, 0.05f, 0.0f, 500.0f, "%.2f");

  ImGui::DragFloat((std::string("減衰(decay)##") + label).c_str(), &data_.decay,
                   0.05f, 0.0f, 32.0f, "%.2f");

  // cosAngle から度数に戻して編集
  float c = std::clamp(data_.cosAngle, -1.0f, 1.0f);
  float angleDeg = RadToDeg_(std::acos(c));

  if (ImGui::DragFloat((std::string("角度(度)##") + label).c_str(), &angleDeg,
                       0.1f, 0.0f, 89.9f, "%.1f")) {
    SetAngleDeg(angleDeg);
  }

  ImGui::TextDisabled("※ intensity<=0 or distance<=0 なら実質OFFにできるよ");
}

} // namespace RC
