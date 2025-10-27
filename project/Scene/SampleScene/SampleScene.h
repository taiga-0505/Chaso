#pragma once
#include "Camera/CameraController.h"
#include "Scene.h"
#include "Sprite2D/Sprite2D.h"
#include "Texture/TextureManager/TextureManager.h"
#include "model3D/model3D.h"
#include <dinput.h>
#include "Sound/Sound.h"
#include "GraphicsPipeline/GraphicsPipeline.h"

class SampleScene final : public Scene {
public:
  const char *Name() const override { return "Sample"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  Matrix4x4 view_, proj_;

  Sound *sound = nullptr;

  // カメラ
  CameraController camera_;

  // ===============
  // 3Dモデル
  // ===============

  int plane = -1;
  Transform* planeTransform_;

  int plane2 = -1;
  Transform *plane2Transform_;

  int blockModel = -1;
  int tx_block = -1;

  int model = -1;
  int tx_model = -1;

  // ===============
  // スプライト
  // ===============

  int sprite = -1;

  // ===============
  // マップ関連
  // ===============

  static const int mapWidth = 5;
  static const int mapHeight = 5;

  int map[mapHeight][mapWidth] = {
      {1, 1, 1, 1, 1},
      {1, 0, 0, 0, 1},
      {1, 0, 1, 0, 1},
      {1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1},
  };
};
