#pragma once
#include "MapChipField/MapChipField.h"
#include "Player/Player.h"
#include "RC.h"
#include "Scene.h"
#include "memory.h"
#include <dinput.h>

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

  // ===== 追従カメラ（メインカメラ用） =====
  RC::Vector3 camPos_ = {0, 0, 0};
  RC::Vector3 prevPlayerPos_ = {0, 0, 0};
  bool camFollowInit_ = false;

  // 補間用の注視点＆先読みの現在値
  RC::Vector3 camFocus_ = {0, 0, 0};
  float camLookAheadX_ = 0.0f;

  // プレイヤー基準
  RC::Vector3 camOffset_ = {
      0.0f, 2.0f, -30.0f}; // カメラ位置 = focus + (camOffset - camTargetOffset)
  RC::Vector3 camTargetOffset_ = {0.0f, 2.0f,
                                  0.0f}; // 注視点（プレイヤー中心より少し上）

  // 追従のなめらかさ
  float camSharpness_ = 15.0f;

  // 先読み（横移動方向）
  float camLookAhead_ = 2.0f;
  float camLookAheadFactor_ = 10.0f;

  // 先読み＆注視点のなめらかさ
  float camLookAheadSharpness_ =
      6.0f; // 小さいほど「戻り」がゆっくりでガクりにくい
  float camFocusSharpness_ = 10.0f; // 注視点の追従（回転のガクッ対策）

    // ===== ジャンプ時の縦追従を抑える（酔い対策）=====
  float camDeadZoneY_ = 1.2f;       // この範囲内はY追従しない（ワールド単位）
  float camYSharpnessUp_ = 2.0f;    // 上方向はゆっくり
  float camYSharpnessDown_ = 12.0f; // 落下方向は早め（見失い防止）
  float camYMaxSpeed_ = 6.0f;       // 1秒あたりの最大Y移動量（0で無効）

  // ===== マップ境界クランプ用 =====
  struct Rect2 {
    float left, right, bottom, top;
  };
  Rect2 mapBounds_ = {0, 0, 0, 0};
  bool mapBoundsReady_ = false;

  // 視野計算用（OnEnterでセット）
  float camFovY_ = 0.45f;
  float camAspect_ = 16.0f / 9.0f;

  // ブロック
  int blockModel = -1;

  // ===== MapChipField =====
  MapChipField map_;
  float kBlockSize = 1.0f;

  // プレイヤー
  int playerModel = -1;
  std::unique_ptr<Player> player_;

  // ===== 天球 =====
  int skydomeModel = -1;
  int txSphere_ = -1;
  Transform *sphereT_ = nullptr;
};
