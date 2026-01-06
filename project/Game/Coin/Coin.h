#pragma once
#include "Math/MathTypes.h"
#include "RC.h"
#include "SceneManager.h"
#include "struct.h"

class Coin {
public:
  void Init(int modelIndex, SceneContext &ctx);
  void Update();
  void Draw();

  void GetCoin();

  void SetWorldPos(const RC::Vector3 &pos);
  int ModelHandle() const { return model_; }

private:
  SceneContext *ctx_ = nullptr;

  int model_ = -1;
  Transform *transform_ = nullptr;

  bool isAlive_ = true;
};
