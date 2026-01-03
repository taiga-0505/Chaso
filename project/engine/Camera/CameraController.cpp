#include "CameraController.h"
#include "imGui/imGui.h"

namespace RC {

// 画角やクリップは共通、初期姿勢だけ可変にしておきます
void CameraController::Initialize(Input *input, const Vector3 &mainPos,
                                  const Vector3 &mainRot, float fovY,
                                  float aspect, float nearZ, float farZ) {
  input_ = input;
  debug_.Initialize(input_, fovY, aspect, nearZ, farZ);
  main_.Initialize(mainPos, mainRot, fovY, aspect, nearZ, farZ);
}

// TAB 切替と各カメラの更新
void CameraController::Update() {
  // 切替
  if (input_ && input_->IsKeyTrigger(DIK_TAB)) {
    useDebug_ = !useDebug_;
  }
  if (useDebug_) {
    if (!(ImGui::GetIO().WantCaptureMouse ||
          ImGui::GetIO().WantCaptureKeyboard)) {
      debug_.Update();
    }
  } else {
    main_.Update();
  }

  if (useDebug_) {
    worldPos_ = debug_.GetPosition();
  } else {
    worldPos_ = main_.GetPosition();
  }
}

void CameraController::DrawImGui() {
#if RC_ENABLE_IMGUI
  ImGui::Begin("カメラモード : Tab");

  if (useDebug_) {
    ImGui::Text("デバッグカメラモード");
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    if (ImGui::Button("リセット")) {
      debug_.Reset(); // 位置・回転を初期値へ
    }
    ImGui::SameLine();
    if (ImGui::Button("操作方法")) {
      showGuide_ = !showGuide_;
    }
  } else {
    ImGui::Text("メインカメラモード");
  }
  ImGui::End();

  // 操作ガイド
  if (showGuide_) {
    ImGui::Begin("操作方法", &showGuide_, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text(" WASD : 前後左右移動");
    ImGui::Text(" QE : 上下移動");
    ImGui::Text(" 十字キー : カメラの回転");
    ImGui::Text(" マウスホイール : 前後移動");
    ImGui::Text(" 左クリックしながらドラッグ : 回転");
    ImGui::Text(" マウスホイール押しながらドラッグ : 上下左右移動");
    ImGui::End();
  }

#endif
}

void CameraController::SetMainPosition(const Vector3 &pos) {
  main_.SetPosition(pos);
}

void CameraController::SetMainRotation(const Vector3 &rot) {
  main_.SetRotation(rot);
}

void CameraController::SetDebugPosition(const Vector3 &pos) {
  debug_.SetPosition(pos);
  if (useDebug_) {
    worldPos_ = pos;
  }
}

void CameraController::SetDebugRotation(const Vector3 &rot) {
  debug_.SetRotation(rot);
}

void CameraController::SetDebugTransform(const Vector3 &pos,
                                         const Vector3 &rot) {
  debug_.SetTransform(pos, rot);
  if (useDebug_) {
    worldPos_ = pos;
  }
}

void CameraController::SetPosition(const Vector3 &pos) {
  if (useDebug_) {
    debug_.SetPosition(pos);
  } else {
    main_.SetPosition(pos);
  }
  worldPos_ = pos; // アクティブ側の位置を即反映
}

void CameraController::SetRotation(const Vector3 &rot) {
  if (useDebug_) {
    debug_.SetRotation(rot);
  } else {
    main_.SetRotation(rot);
  }
}

void CameraController::SetTransform(const Vector3 &pos, const Vector3 &rot) {
  if (useDebug_) {
    debug_.SetTransform(pos, rot);
  } else {
    main_.SetPosition(pos);
    main_.SetRotation(rot);
  }
  worldPos_ = pos;
}
} // namespace RC
