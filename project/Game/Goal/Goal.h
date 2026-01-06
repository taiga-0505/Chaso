#pragma once
#include "Math/MathTypes.h"
#include "RC.h"
#include "struct.h"

class Goal {
public:
  void Init(int modelIndex);
  void Update();
  void Draw();

  RC::Vector3 GetWorldPos() const;
  void SetWorldPos(const RC::Vector3 &pos);

  // 多重遷移防止用
  void Reach();
  bool IsReached() const { return reached_; }

  int ModelHandle() const { return model_; }

private:
  int model_ = -1;
  Transform *transform_ = nullptr;

  bool reached_ = false;
  float t_ = 0.0f;
  RC::Vector3 basePos_ = {};
};
