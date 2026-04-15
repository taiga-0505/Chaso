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

  void GetCoin(); // “回収開始”にする
  bool IsAlive() const { return isAlive_; }
  bool IsCollected() const { return collected_; }
  RC::Vector3 GetWorldPos() const;

  void SetWorldPos(const RC::Vector3 &pos);
  int ModelHandle() const { return model_; }

private:
  SceneContext *ctx_ = nullptr;

  int model_ = -1;
  Transform *transform_ = nullptr;

   bool isAlive_ = true;
  bool collected_ = false;
  float collectedT_ = 0.0f;
  RC::Vector3 collectedStartPos_ = {};
};
