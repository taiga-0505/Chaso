#pragma once
#include "Math/MathTypes.h"
#include "SceneManager.h"
#include "struct.h"

class Player {
public:

  void Init(int modelIndex, SceneContext &ctx);
  void Update();
  void Draw();

private:
  void Move();
  void Jump();

public:
private:
  SceneContext *ctx_ = nullptr;

  int model_ = -1;

  Transform *transform_ = nullptr;

  RC::Vector3 velocity_ = {};

  float moveSpeed_ = 0.1f;
  float jumpStrength_ = 0.2f;
  float gravity_ = 0.05f;
};
