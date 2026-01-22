#pragma once
#include "Scene.h"
#include <dinput.h>
#include <RC.h>

class SampleScene final : public Scene {
public:
  const char *Name() const override { return "Sample"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  void DrawImGui();

private:
  RC::Matrix4x4 view_, proj_;

  // カメラ
  RC::CameraController camera_;

  // Light
  int light = -1;

  // ===============
  // 3Dモデル
  // ===============

  int plane = -1;
  Transform *planeTransform_;

  int blockModel = -1;
  int tx_block = -1;

  int model = -1;
  int tx_model = -1;

  // ===== 天球 =====
  int sphere = -1;
  int tx_Sphere_ = -1;
  Transform *sphereT_ = nullptr;

  int ball = -1;
  int tx_ball = -1;
  Transform *ballT_ = nullptr;

  // ===============
  // スプライト
  // ===============

  int sprite = -1;

  float t = 0.0f;

  RC::Vector4 fogColor_ = {0.85f, 0.92f, 1.00f, 1.0f};
  bool isFogEnabled_ = true;
};
