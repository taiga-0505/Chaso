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

  // ==============
  // ブロック・マップ関連
  // ==============
  // ======= ブロックモデル =======
  int blockModel = -1;
  // ======= マップフィールド =======
  MapChipField map_;
  // ブロックサイズ
  float kBlockSize = 1.0f;

  // ==============
  // プレイヤー関連
  // ==============
  // ======= プレイヤーモデル =======
  int playerModel = -1;
  // ======= プレイヤーインスタンス =======
  std::unique_ptr<Player> player_;

  // ==============
  // コイン関連
  // ==============
  // ======= コインコンテナ =======
  std::vector<std::unique_ptr<Coin>> coins_;

  // ==============
  // ゴール関連
  // ==============
  // ======= ゴールモデル =======
  std::vector<std::unique_ptr<Goal>> goals_;
  bool reachedGoal_ = false;

  // ==============
  // スカイドーム関連
  // ==============
  // ======= モデルとテクスチャ =======
  int skydomeModel = -1;
  int txSphere_ = -1;

  float skydomeTranslateY_ = 10.0f;
  float skydomeRotateSpeed_ = 0.0005f;
  // ======= トランスフォーム参照 =======
  Transform *sphereT_ = nullptr;
};
