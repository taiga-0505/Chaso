#pragma once

#include "Math/Math.h" // Transform, Vector4
#include "Scene.h"     // SceneContext
#include <d3d12.h>

class Fade {
public:
  enum class Status {
    None,
    FadeIn,
    FadeOut,
    kOverlay,
  };

  void Init(SceneContext &ctx, float width, float height);
  void Term();

  void Start(Status status, float duration, float alpha = 0.5f);
  void Stop();

  void Update();
  void
  Draw(ID3D12GraphicsCommandList *cl); // cl は互換のため残す（中では使わない）

  bool IsFinished() const;

private:
  Status status_ = Status::None;

  int fadeSpriteHandle_ = -1;

  float duration_ = 0.0f;
  float counter_ = 0.0f;
  float overlayAlpha_ = 0.0f;

  float width_ = 0.0f;
  float height_ = 0.0f;
};
