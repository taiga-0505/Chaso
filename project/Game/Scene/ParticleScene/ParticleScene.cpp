#include "ParticleScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void ParticleScene::OnEnter(SceneContext &ctx) {
  // =============================
  // Camera
  // =============================

  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.0f, -10.0f},
                     RC::Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  particle_.Initialize(ctx);
}

void ParticleScene::OnExit(SceneContext &ctx) {

  particle_.Finalize();
}

void ParticleScene::Update(SceneManager &sm, SceneContext &ctx) {

  particle_.DrawImGui();

  // ==============================
  // 操作説明（ImGui）
  // ==============================
  // 左上に固定（少しだけ余白を空けたいなら (10,10) とか）
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
  // サイズも固定したいなら（お好みで調整）
  ImGui::SetNextWindowSize(ImVec2(420.0f, 220.0f), ImGuiCond_Always);

  // ウィンドウ用フラグ
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar |     // タイトルバー無し
      ImGuiWindowFlags_NoResize |       // リサイズ不可
      ImGuiWindowFlags_NoMove |         // 移動不可
      ImGuiWindowFlags_NoCollapse |     // 畳めない
      ImGuiWindowFlags_NoSavedSettings; // 位置/サイズを保存しない（毎回固定）

  // このウィンドウだけ角を丸くする
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                      10.0f); // 角丸半径（お好みで）

  ImGui::Begin("パーティクルシーンの操作", nullptr, flags);

  ImGui::Text("パーティクル操作");
  ImGui::BulletText("Spaceキー : Update の一時停止 / 再開");
  ImGui::BulletText("1キー      : 風(加速度フィールド)の ON / OFF 切り替え");
  ImGui::BulletText("2キー      : 全パーティクルを最大数でリスポーン");
  ImGui::BulletText("3キー      : 全パーティクルを全削除");
  ImGui::BulletText("4キー      : エミッタからパーティクルを追加発生");

  ImGui::Separator();
  ImGui::Text("エミッタ移動（キーを押している間、移動し続けます）");
  ImGui::BulletText("A / D キー : X軸 方向に移動（A: -X, D: +X）");
  ImGui::BulletText("W / S キー : Y軸 方向に移動（W: +Y, S: -Y）");
  ImGui::BulletText("Q / E キー : Z軸 方向に移動（Q: +Z, E: -Z）");

  ImGui::End();

  // ===========================================
  // 更新処理
  // ===========================================

  camera_.Update();

  // viewとprojを渡す
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_);

  // ==============================
  // キー入力でパーティクル操作
  // ==============================
  Input *input = ctx.input;

  // Space : Update を止めたり / 再開したり
  if (input->IsKeyTrigger(DIK_SPACE)) {
    particle_.ToggleEnableUpdate();
  }

  // 1キー : 風（加速度フィールド）ON/OFF
  if (input->IsKeyTrigger(DIK_1)) {
    particle_.ToggleUseAccelerationField();
  }

  // 3キー : 全部リスポーン（最大数で再生成）
  if (input->IsKeyTrigger(DIK_2)) {
    particle_.RespawnAllMax();
  }

  // 3キー : 全パーティクル削除
  if (input->IsKeyTrigger(DIK_3)) {
    particle_.ClearAll();
  }

  // 4キー : エミッタからパーティクル追加発生
  if (input->IsKeyTrigger(DIK_4)) {
    particle_.AddParticlesFromEmitter();
  }

  // IJKL + U/O でエミッタの位置を動かす
  //   A : -X方向, D : +X方向
  //   W : +Y方向, S : -Y方向
  //   Q : +Z方向, E : -Z方向
  const float emitterMoveSpeed = 0.05f;
  RC::Vector3 emitterDelta{0.0f, 0.0f, 0.0f};

  if (input->IsKeyPressed(DIK_A)) {
    emitterDelta.x -= emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_D)) {
    emitterDelta.x += emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_W)) {
    emitterDelta.y += emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_S)) {
    emitterDelta.y -= emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_Q)) {
    emitterDelta.z += emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_E)) {
    emitterDelta.z -= emitterMoveSpeed;
  }

  if (emitterDelta.x != 0.0f || emitterDelta.y != 0.0f ||
      emitterDelta.z != 0.0f) {
    particle_.MoveEmitter(emitterDelta);
  }

  particle_.Update(view_, proj_);
}

void ParticleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);
  
  particle_.Render(ctx,cl);

  RC::PreDraw2D(ctx, cl);
}
