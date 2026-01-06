#pragma once
#include "DebugCamera/DebugCamera.h"
#include "Input/Input.h"
#include "MainCamera/MainCamera.h"
#include "Math/Math.h"

// Transform は別ヘッダで定義されている想定（ここではポインタだけ扱う）
struct Transform;

namespace RC {

struct CameraMatrices {
  Matrix4x4 view;
  Matrix4x4 proj;
};

class CameraController {
public:
  // 画角やクリップは共通、初期姿勢だけ可変にしておきます
  void Initialize(Input *input, const Vector3 &mainPos, const Vector3 &mainRot,
                  float fovY, float aspect, float nearZ, float farZ);

  // TAB 切替と各カメラの更新
  // dt は追従カメラの補間用。指定しない場合は 60fps 想定。
  void Update(float dt = 1.0f / 60.0f);

  // いま有効なカメラの行列
  const Matrix4x4 &GetView() const {
    return useDebug_ ? debug_.GetView() : main_.GetView();
  }
  const Matrix4x4 &GetProjection() const {
    return useDebug_ ? debug_.GetProjection() : main_.GetProjection();
  }

  CameraMatrices GetMatrices() const {
    if (useDebug_) {
      return {debug_.GetView(), debug_.GetProjection()};
    } else {
      return {main_.GetView(), main_.GetProjection()};
    }
  }

  void DrawImGui();

  // 外側から（例えばボタンで）モードを明示切替したい場合用
  void SetUseDebug(bool v) { useDebug_ = v; }
  bool IsUsingDebug() const { return useDebug_; }

  // --- 追従用：対象を渡したらメインカメラが追従モードになる ---
  // ※ target == nullptr なら追従オフ
  void SetTarget(const ::Transform *target);
  void ClearTarget() { SetTarget(nullptr); }
  bool HasTarget() const { return target_ != nullptr; }

  // --- 追従パラメータ（必要になったら GameScene 側からいじれる） ---
  void SetFollowOffsets(const Vector3 &camOffset, const Vector3 &targetOffset);
  void SetFollowSharpness(float camSharpness);
  void SetLookAhead(float lookAhead, float lookAheadFactor,
                    float lookAheadSharpness);
  void SetFocusSharpness(float focusSharpness);
  void SetFollowYSettings(float deadZoneY, float sharpnessUp,
                          float sharpnessDown, float maxSpeed);

  // マップ境界クランプ（ワールド座標）
  void SetFollowBounds(float left, float right, float bottom, float top,
                       bool enable);

  // --- 外部から（例えば演出で）メインカメラの姿勢を更新 ---
  void SetMainPosition(const Vector3 &pos);
  void SetMainRotation(const Vector3 &rot);

  RC::Vector3 GetWorldPos() const { return worldPos_; }

  // ==================
  // デバッグカメラを外部から直接いじる
  // ==================
  void SetDebugPosition(const Vector3 &pos);
  void SetDebugRotation(const Vector3 &rot);
  void SetDebugTransform(const Vector3 &pos, const Vector3 &rot);

  // ==================
  // いま有効なカメラ（Main/Debug）を同じAPIでいじる
  // ==================
  void SetPosition(const Vector3 &pos);
  void SetRotation(const Vector3 &rot);
  void SetTransform(const Vector3 &pos, const Vector3 &rot);

private:
  void UpdateFollow_(float dt);

  // ===== input / cameras =====
  Input *input_ = nullptr;
  DebugCamera debug_;
  MainCamera main_;
  bool useDebug_ = false;
  bool showGuide_ = false;

  RC::Vector3 worldPos_{0, 0, 0};

  // ===== follow camera =====
  const ::Transform *target_ = nullptr;

  struct FollowBounds2D {
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
    bool enabled = false;
  } bounds_;

  struct FollowParams {
    // カメラ位置 = focus + (camOffset - targetOffset)
    Vector3 camOffset = {0.0f, 0.0f, -30.0f};
    Vector3 targetOffset = {0.0f, 2.0f, 0.0f};

    // 追従のなめらかさ
    float camSharpness = 15.0f;

    // 先読み（横移動方向）
    float lookAhead = 2.0f;
    float lookAheadFactor = 10.0f;

    // 先読み＆注視点のなめらかさ
    float lookAheadSharpness = 6.0f;
    float focusSharpness = 10.0f;

    // ジャンプ時の縦追従を抑える（酔い対策）
    float deadZoneY = 1.2f;
    float ySharpnessUp = 2.0f;
    float ySharpnessDown = 12.0f;
    float yMaxSpeed = 6.0f; // 0で無効
  } followParams_;

  struct FollowState {
    Vector3 camPos = {0.0f, 0.0f, 0.0f};
    Vector3 prevTargetPos = {0.0f, 0.0f, 0.0f};
    bool initialized = false;

    Vector3 focus = {0.0f, 0.0f, 0.0f};
    float lookAheadX = 0.0f;
  } follow_;

  float fovY_ = 0.45f;
  float aspect_ = 16.0f / 9.0f;
};

} // namespace RC
