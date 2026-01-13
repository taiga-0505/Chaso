#pragma once
#include "MapChipField/MapChipField.h"
#include "Player/Player.h"
#include "RC.h"
#include "Scene.h"
#include "memory.h"
#include <dinput.h>
#include "Coin/Coin.h"
#include <vector>
#include "Goal/Goal.h"

class GameScene final : public Scene {
public:
  const char *Name() const override { return "Game"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  // ==============
  // カメラ関連
  // ==============
  // ======= ビュー/プロジェクション行列 =======
  RC::Matrix4x4 view_, proj_;
  // ======= カメラコントローラ =======
  RC::CameraController camera_;

};
