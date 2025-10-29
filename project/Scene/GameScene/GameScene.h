#pragma once
#include "RC.h"
#include "Scene.h"
#include <dinput.h>
#include "MapChipField/MapChipField.h"
#include "Player/Player.h"
#include "memory.h"

class GameScene final : public Scene {
public:
  const char *Name() const override { return "Game"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  RC::Matrix4x4 view_, proj_;

  // カメラ
  RC::CameraController camera_;

  // ブロック
  int blockModel = -1;

  // ===== MapChipField =====
  MapChipField map_;
  float kBlockSize = 1.0f;

  // プレイヤー
  int playerModel = -1;
  std::unique_ptr<Player> player_;
};
