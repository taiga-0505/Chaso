#include "ParticleScene.h"
#include "Dx12/Dx12Core.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include "Particle/ParticleManager.h"
#include "Particle/Flash/FlashParticle.h"
#include "Particle/Ring/RingParticle.h"
#include "Particle/Cylinder/CylinderParticle.h"

void ParticleScene::OnEnter(SceneContext &ctx) {
  // =============================
  // Camera
  // =============================
  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.0f, -10.0f},
                     RC::Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  auto& pm = RC::ParticleManager::GetInstance();
  pm.ClearSystems();

  auto initSys = [&](const std::string& name, std::unique_ptr<RC::Particle> sys) {
      sys->Initialize(ctx);
      pm.RegisterSystem(name, std::move(sys));
  };

  initSys("Particle", std::make_unique<RC::Particle>());
  initSys("Fire", std::make_unique<RC::FireParticle>());
  initSys("Rain", std::make_unique<RC::RainParticle>());
  initSys("Snow", std::make_unique<RC::SnowParticle>());
  initSys("Circle", std::make_unique<RC::CircleParticle>());
  
  auto exp = std::make_unique<RC::ExplosionParticle>();
  exp->SetEmitterAutoSpawn(false);
  initSys("Explosion", std::move(exp));

  initSys("Laser", std::make_unique<RC::LaserParticle>());

  auto impact = std::make_unique<RC::ImpactSparkParticle>();
  impact->SetEmitterAutoSpawn(false);
  initSys("ImpactSpark", std::move(impact));

  auto wind = std::make_unique<RC::WindParticle>();
  wind->SetActive(false);
  initSys("Wind", std::move(wind));

  auto flash = std::make_unique<RC::FlashParticle>();
  flash->SetEmitterAutoSpawn(false);
  initSys("Flash", std::move(flash));

  initSys("Ring", std::make_unique<RC::RingParticle>());
  initSys("Cylinder", std::make_unique<RC::CylinderParticle>());


  RC::EffectPreset hitEffect;
  hitEffect.name = "Hit";
  hitEffect.AddEmitter("Flash", 5, 1.0f);
  hitEffect.AddEmitter("Ring", 1, 1.0f);
  hitEffect.AddEmitter("Cylinder", 1, 1.0f);
  pm.RegisterPreset(hitEffect);

  impactDesc_.countPerTick = 6;
  impactDesc_.burstOnStart = 24;

  guide = RC::LoadSprite("Resources/guide.png", ctx, true);
  RC::SetSpriteScreenSize(guide, 330, 200);

  int whiteTex = RC::LoadTex("Resources/white1x1.png", true);
  D3D12_GPU_DESCRIPTOR_HANDLE whiteSrv = RC::GetSrv(whiteTex);
}

void ParticleScene::OnExit(SceneContext &ctx) {
  RC::ParticleManager::GetInstance().ClearSystems();
  RC::UnloadSprite(guide);
  guide = -1;
}

void ParticleScene::Update(SceneManager &sm, SceneContext &ctx) {
#if RC_ENABLE_IMGUI

  auto& pm = RC::ParticleManager::GetInstance();
  if (pm.GetSystem("Particle")) {
      pm.GetSystem("Particle")->DrawImGui();
  }

  // ===========================================
  // 更新処理
  // ===========================================

  camera_.Update();

  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

  Input *input = ctx.input;

  auto setMode = [&](const std::string& modeName, const std::vector<std::string>& systems) {
      currentMode_ = modeName;
      activeSystems_ = systems;
  };

  if (input->IsKeyTrigger(DIK_1)) {
    setMode("Particle", {"Particle"});
    if(auto* s = pm.GetSystem("Particle")) s->RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_2)) {
    setMode("Fire", {"Fire"});
    if(auto* s = pm.GetSystem("Fire")) s->RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_3)) {
    setMode("Rain", {"Rain"});
    if(auto* s = pm.GetSystem("Rain")) s->RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_4)) {
    setMode("Snow", {"Snow"});
    if(auto* s = pm.GetSystem("Snow")) s->RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_5)) {
    setMode("Explosion", {"Explosion"});
    if(auto* s = pm.GetSystem("Explosion")) s->RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_6)) {
    setMode("Circle", {"Circle"});
    if(auto* s = pm.GetSystem("Circle")) s->RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_7)) {
    setMode("Laser", {"Laser", "ImpactSpark"});
  }
  if (input->IsKeyTrigger(DIK_8)) {
    setMode("Wind", {"Wind"});
    if(auto* s = dynamic_cast<RC::WindParticle*>(pm.GetSystem("Wind"))) {
      s->SetWind(windOrigin_, windForce_, windRange_, windRadius_, true);
    }
  }
  if (input->IsKeyTrigger(DIK_9)) {
    setMode("HitEffect", {"HitSparks", "Flash", "Ring", "Cylinder"});
  }
  if (input->IsKeyTrigger(DIK_0)) {
    setMode("Cylinder", {"Cylinder"});
    if(auto* s = pm.GetSystem("Cylinder")) s->RespawnAllMax();
  }

  const float emitterMoveSpeed = 0.05f;
  RC::Vector3 emitterDelta{0.0f, 0.0f, 0.0f};

  if (input->IsKeyPressed(DIK_A)) emitterDelta.x -= emitterMoveSpeed;
  if (input->IsKeyPressed(DIK_D)) emitterDelta.x += emitterMoveSpeed;
  if (input->IsKeyPressed(DIK_W)) emitterDelta.y += emitterMoveSpeed;
  if (input->IsKeyPressed(DIK_S)) emitterDelta.y -= emitterMoveSpeed;
  if (input->IsKeyPressed(DIK_Q)) emitterDelta.z += emitterMoveSpeed;
  if (input->IsKeyPressed(DIK_E)) emitterDelta.z -= emitterMoveSpeed;

  if (emitterDelta.x != 0.0f || emitterDelta.y != 0.0f || emitterDelta.z != 0.0f) {
    for (const auto& sysName : activeSystems_) {
      if (auto* s = pm.GetSystem(sysName)) {
        s->MoveEmitter(emitterDelta);
      }
    }
  }

  if (input->IsKeyTrigger(DIK_SPACE)) {
    if (currentMode_ == "Explosion") {
      if (auto* s = dynamic_cast<RC::ExplosionParticle*>(pm.GetSystem("Explosion"))) {
          s->Trigger(RC::Vector3{0.0f, 0.0f, 0.0f});
      }
    } else if (currentMode_ == "Cylinder") {
      if (auto* s = dynamic_cast<RC::CylinderParticle*>(pm.GetSystem("Cylinder"))) {
          s->Trigger(RC::Vector3{0.0f, 0.0f, 0.0f});
      }
    }
  }

  if (currentMode_ == "HitEffect") {
    ImGui::Begin("HitEffect Control");
    static RC::Vector3 hitPos{0.0f, 0.0f, 0.0f};
    ImGui::DragFloat3("Position", &hitPos.x, 0.1f);
    if (ImGui::Button("Trigger HitEffect")) {
      pm.PlayEffect("Hit", hitPos);
    }
    ImGui::End();
  }

  auto* laserSys = dynamic_cast<RC::LaserParticle*>(pm.GetSystem("Laser"));
  auto* impactSys = dynamic_cast<RC::ImpactSparkParticle*>(pm.GetSystem("ImpactSpark"));

  if (currentMode_ == "Laser" && laserSys && impactSys) {
    ImGui::Begin("Laser Control");
    ImGui::DragFloat3("Start", &laserStart_.x, 0.1f);
    ImGui::DragFloat3("End", &laserEnd_.x, 0.1f);
    ImGui::DragFloat("Width", &laserWidth_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Life", &laserLife_, 0.01f, 0.01f, 10.0f);
    ImGui::ColorEdit4("Color", &laserColor_.x);
    ImGui::Separator();

    ImGui::Text("streak settings");
    ImGui::DragInt("Streak Count", &streakCount_, 1, 1, 500);
    ImGui::DragFloat("Streak Min Length", &streakMinLen_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Streak Max Length", &streakMaxLen_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Streak Width", &streakWidth_, 0.001f, 0.001f, 1.0f);
    ImGui::DragFloat("Streak Spread Z Ratio", &streakSpreadZRatio_, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();

    ImGui::Text("impact particle settings");
    ImGui::DragFloat("Impact Interval", &impactDesc_.interval, 0.01f, 0.0f, 1.0f);
    ImGui::DragInt("Count Per Tick", &impactDesc_.countPerTick, 1, 1, 100);
    ImGui::DragInt("Burst On Start", &impactDesc_.burstOnStart, 1, 0, 100);

    ImGui::End();

    laserSys->SetStyle(RC::LaserParticle::Style::Streak);
    laserSys->SetStreakParams(streakCount_, streakMinLen_, streakMaxLen_, streakWidth_, streakSpreadZRatio_);
    laserSys->SetBeam(laserStart_, laserEnd_, laserWidth_, laserLife_, laserColor_);
    
    impactSys->SetImpactFromBeam(laserStart_, laserEnd_, impactDesc_);
  } else if (impactSys) {
    impactSys->StopImpact();
  }

  // Update registered and active systems
  for (const auto& sysName : activeSystems_) {
    if (auto* s = pm.GetSystem(sysName)) {
      s->SetDeltaTime(ctx.deltaTime);
      s->Update(view_, proj_);
    }
  }

#endif
}

void ParticleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);

  auto& pm = RC::ParticleManager::GetInstance();

  for (const auto& sysName : activeSystems_) {
    if (auto* s = pm.GetSystem(sysName)) {
      s->Render(ctx, cl);
    }
  }

  RC::PreDraw2D(ctx, cl);
  RC::DrawSprite(guide);
}

ParticleScene::~ParticleScene() {
  if (guide >= 0) {
    RC::UnloadSprite(guide);
    guide = -1;
  }
}
