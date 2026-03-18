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
  ~SampleScene() override;

private:
  RC::Matrix4x4 view_, proj_;

  // カメラ
  RC::CameraController camera_;

  // Light
  int directionalLight = -1;

  int pointLight = -1;
  int pointLight2 = -1;

  int spotLight = -1;
  int spotLight2 = -1;

  // ===============
  // 3Dモデル
  // ===============

  int plane = -1;
  Transform *planeTransform_ = nullptr;

  int blockModel = -1;
  RC::Vector4 blockColor_ = {0.8f, 0.9f, 1.0f, 0.12f};

  int model = -1;
  int tx_model = -1;

  int terrain = -1;
  Transform *terrainT_ = nullptr;

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

  RC::Vector4 fogColor_ = {0.1f, 0.45f, 0.9f, 0.35f};
  bool isFogEnabled_ = false;

  Transform spriteTransform_;
  RC::Vector2 spriteSize_ = {100.0f, 100.0f};
};
