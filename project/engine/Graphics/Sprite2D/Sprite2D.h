#pragma once
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>

#include "Math/Math.h"
#include "function/function.h"
#include "imgui/imgui.h"

#include "SpriteMesh2D.h" // ★追加（SpriteVertex もここに移動）

class Sprite2D {
public:
  Sprite2D() = default;
  ~Sprite2D();

  // ★mesh を受け取るように変更
  void Initialize(ID3D12Device *device,
                  const std::shared_ptr<SpriteMesh2D> &mesh, float screenWidth,
                  float screenHeight);

  void Update();
  void Draw(ID3D12GraphicsCommandList *cmdList) const;

  // ===== setters / getters =====
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { srv_ = srv; }
  void SetScreenSize(float w, float h);
  void SetSize(float w, float h);
  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }
  void SetColor(const RC::Vector4 &color) {
    cbMat_.map->color = RC::Vector4(color.x, color.y, color.z, color.w);
  }
  RC::Vector4 GetColor() { return cbMat_.map->color; }

  Transform &T() { return transform_; }
  SpriteMaterial *Mat() { return cbMat_.map; }
  RC::Matrix4x4 &UVTransform() { return cbMat_.map->uvTransform; }

  void DrawImGui(const char *name);

private:
  struct CBW {
    ID3D12Resource *res = nullptr;
    TransformationMatrix *map = nullptr;
  };
  struct CBM {
    ID3D12Resource *res = nullptr;
    SpriteMaterial *map = nullptr;
  };

  void Release();

private:
  ID3D12Device *device_ = nullptr;

  // ★共通メッシュ（VB/IB）はここで共有
  std::shared_ptr<SpriteMesh2D> mesh_;

  float screenW_ = 0.0f;
  float screenH_ = 0.0f;

  CBW cbWVP_{};
  CBM cbMat_{};

  D3D12_GPU_DESCRIPTOR_HANDLE srv_{};

  RC::Matrix4x4 view_{};
  RC::Matrix4x4 proj_{};

  Transform transform_{{100, 100, 1}, {0, 0, 0}, {0, 0, 0}};
  bool visible_ = true;
};
