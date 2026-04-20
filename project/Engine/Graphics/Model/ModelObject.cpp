#include "ModelObject.h"
#include "Common/Log/Log.h"
#include "Render/FrameResource.h"
#include "Texture/TextureManager/TextureManager.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>

using namespace RC;
namespace fs = std::filesystem;

void ModelObject::Initialize(ID3D12Device *device) {
  resource_.Initialize(device);

  // 初期ライティングを反映
  ApplyLightingIfReady_();
}

void ModelObject::EnsureSphericalUVIfMissing() {
  if (resource_.GetMesh()) {
    resource_.GetMesh()->EnsureSphericalUVIfMissing();
  }
}

void ModelObject::SetColor(const Vector4 &color) {
  if (Material *mat = resource_.Mat()) {
    mat->color = color;
  }
}

void ModelObject::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  cachedView_ = view;
  cachedProj_ = proj;
  hasVP_ = true;
}

void ModelObject::Draw(ID3D12GraphicsCommandList *cmdList,
                       const Matrix4x4 &world, FrameResource &frame) {
  if (!visible_)
    return;

  // View/Proj が無いなら単位行列で描く（事故防止）
  const Matrix4x4 view = hasVP_ ? cachedView_ : MakeIdentity4x4();
  const Matrix4x4 proj = hasVP_ ? cachedProj_ : MakeIdentity4x4();

  resource_.Draw(cmdList, world, view, proj, frame);
}

void ModelObject::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                            const Matrix4x4 &view, const Matrix4x4 &proj,
                            const std::vector<Transform> &instances,
                            FrameResource &frame) {
  resource_.DrawBatch(cmdList, view, proj, instances, frame);
}

void ModelObject::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                            const Matrix4x4 &view, const Matrix4x4 &proj,
                            const std::vector<Transform> &instances,
                            const RC::Vector4 &color,
                            FrameResource &frame) {
  resource_.DrawBatch(cmdList, view, proj, instances, color, frame);
}

ModelObject &ModelObject::SetLightingConfig(LightingMode mode,
                                            const std::array<float, 3> &color,
                                            const std::array<float, 3> &dir,
                                            float intensity) {
  initialLighting_.mode = mode;
  initialLighting_.color[0] = color[0];
  initialLighting_.color[1] = color[1];
  initialLighting_.color[2] = color[2];

  initialLighting_.dir[0] = dir[0];
  initialLighting_.dir[1] = dir[1];
  initialLighting_.dir[2] = dir[2];

  initialLighting_.intensity = intensity;

  ApplyLightingIfReady_();
  return *this;
}

void ModelObject::ApplyLightingIfReady_() {
  resource_.ApplyLighting(static_cast<int>(initialLighting_.mode),
                          initialLighting_.color, initialLighting_.dir,
                          initialLighting_.intensity);
}

// ============================================================================
// ImGui
// ============================================================================

// ツールチップ用のヘルパ
static void HelpMarker(const char *desc) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

// 長い文字列：折り返し + ホバーで全文 + クリックでコピー
static void ShowLongTextJP(const std::string &s) {
  const char *text = s.empty() ? "(なし)" : s.c_str();

  // 今いるセル幅で折り返す
  const float wrap = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
  ImGui::PushTextWrapPos(wrap);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();

  // ホバーで全文だけ出す（コピーはしない）
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void ModelObject::DrawImGui(const char *name, bool showLightingUi) {
#if RC_ENABLE_IMGUI

  const auto &mesh_ = resource_.GetMesh();

  std::string label = name ? std::string(name) : std::string("ModelObject");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;

  bool vis = Visible();
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
    SetVisible(vis);

  if (vis) {
    ImGui::TextUnformatted("Transform");
    ImGui::DragFloat3((std::string("位置(x,y,z)##") + label).c_str(),
                      &transform_.translation.x, 0.1f, -4096.0f, 4096.0f,
                      "%.2f");
    ImGui::SliderAngle((std::string("回転X##") + label).c_str(),
                       &transform_.rotation.x);
    ImGui::SliderAngle((std::string("回転Y##") + label).c_str(),
                       &transform_.rotation.y);
    ImGui::SliderAngle((std::string("回転Z##") + label).c_str(),
                       &transform_.rotation.z);
    ImGui::DragFloat3((std::string("スケール(x,y,z)##") + label).c_str(),
                      &transform_.scale.x, 0.01f, 0.001f, 4096.0f, "%.3f");

    ImGui::Dummy(ImVec2(0, 6));

    DirectionalLight *light = resource_.Light();
    Material *mat = resource_.Mat();

    if (showLightingUi) {
      if (light && mat) {
        ImGui::TextUnformatted("Lighting / Material");
        ImGui::ColorEdit3((std::string("ライト色##") + label).c_str(),
                          &light->color.x, ImGuiColorEditFlags_Float);
        ImGui::DragFloat3((std::string("ライト方向##") + label).c_str(),
                          &light->direction.x, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat((std::string("強さ##") + label).c_str(),
                         &light->intensity, 0.01f, 0.0f, 64.0f);

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                          &mat->color.x, ImGuiColorEditFlags_Float);
      } else {
        ImGui::TextDisabled("Light / Material CB not ready.");
      }
      ImGui::Dummy(ImVec2(0, 6));

      if (mat && mat->lightingMode != None) {
        ImGui::DragFloat((std::string("Shininess##") + label).c_str(),
                         &mat->shininess, 0.5f, 0.0f, 256.0f, "%.1f");
        ImGui::SameLine();
        ImGui::TextDisabled("(0で鏡面なし)");
      }
    }

    if (mesh_) {
      ImGui::TextUnformatted("テクスチャ情報");

      const auto &mats = mesh_->Materials();

      int count = 0;
      if (!mats.empty()) {
        count = (int)mats.size();
      } else if (!mesh_->MaterialFile().textureFilePath.empty()) {
        count = 1;
      }

      const ImGuiTableFlags flags =
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp |
          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg;

      if (ImGui::BeginTable("##TexInfo", 2, flags)) {
        ImGui::TableSetupColumn("項目", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
        ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

        // マテリアル数
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("マテリアル数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", count);

        // 各マテリアルが「今参照してる」テクスチャパス
        const int kMaxShow = 32;
        for (int i = 0; i < count && i < kMaxShow; ++i) {
          std::string path;
          if (!mats.empty()) {
            path = mats[i].textureFilePath;
          } else {
            path = mesh_->MaterialFile().textureFilePath;
          }

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          char labelBuf[64];
          snprintf(labelBuf, sizeof(labelBuf), "[%d] 参照テクスチャ", i);
          ImGui::TextDisabled("%s", labelBuf);

          ImGui::TableSetColumnIndex(1);
          ShowLongTextJP(path);
        }

        if (count > kMaxShow) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("...");
          ImGui::TableSetColumnIndex(1);
          ImGui::TextDisabled("残り %d 件", count - kMaxShow);
        }

        ImGui::EndTable();
      }

      ImGui::Dummy(ImVec2(0, 6));
    }

    // ----------------------------------------------------------
    // モデル参照情報（日本語表示）
    // ----------------------------------------------------------
    if (mesh_) {
      ImGui::TextUnformatted("モデル参照");

      const auto &file = mesh_->SourceFilePath();
      const auto &input = mesh_->SourceInputPath();

      std::string rootName;
      if (!file.empty()) {
        rootName = fs::path(file).filename().string();
      } else if (!input.empty()) {
        rootName = fs::path(input).filename().string();
      } else {
        rootName = "(不明)";
      }

      const ImGuiTableFlags flags =
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp |
          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg;

      if (ImGui::BeginTable("##ModelRef", 2, flags)) {
        ImGui::TableSetupColumn("項目", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
        ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

        auto RowText = [&](const char *k, const std::string &v) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", k);
          ImGui::TableSetColumnIndex(1);
          ShowLongTextJP(v);
        };

        RowText("読み込んだファイル", file);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("共有参照数");

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", (int)mesh_.use_count());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("頂点数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", mesh_->VertexCount());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("描画パーツ数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", (uint32_t)mesh_->DrawItems().size());

        RowText("モデル名", rootName);

        ImGui::EndTable();
      }

      ImGui::Dummy(ImVec2(0, 6));
    }
  }

  ImGui::Dummy(ImVec2(0, 6));

#endif
}
