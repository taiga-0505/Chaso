#include "Particle.h"
#include "Dx12Core.h"
#include "PipelineManager.h"
#include "RenderCommon.h"
#include <Math/Math.h>
#include <cstdio>
#include <function/function.h>
#include <imgui/imgui.h>
#include <numbers>

namespace RC {

void Particle::Initialize(SceneContext &ctx) {

  // ==================
  // デバイス／StructuredBufferManager を取得
  // ＋ ImGui 用パラメータ初期化
  // ==================
  device_ = ctx.core->GetDevice();
  sbMgr_ = &ctx.core->StructuredBuffers();

  uvScale_ = {1.0f, 1.0f};
  uvTranslate_ = {0.0f, 0.0f};
  uvRotate_ = 0.0f;

  // ==================
  // 板ポリゴン用の頂点データ作成
  //   - 左上(-1,  1) 右上(1,  1)
  //   - 左下(-1, -1) 右下(1, -1)
  //   2枚の三角形で1枚の四角形を作る
  // ==================
  modelData.vertices.clear();
  // modelData.vertices.reserve(6);

  // 1枚目: 左上 -> 右上 -> 左下 （時計回り）
  modelData.vertices.push_back({.position = {-1.0f, 1.0f, 0.0f, 1.0f},
                                .texcoord = {0.0f, 0.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 左上

  modelData.vertices.push_back({.position = {1.0f, 1.0f, 0.0f, 1.0f},
                                .texcoord = {1.0f, 0.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 右上

  modelData.vertices.push_back({.position = {-1.0f, -1.0f, 0.0f, 1.0f},
                                .texcoord = {0.0f, 1.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 左下

  // 2枚目: 左下 -> 右上 -> 右下 （時計回り）
  modelData.vertices.push_back({.position = {-1.0f, -1.0f, 0.0f, 1.0f},
                                .texcoord = {0.0f, 1.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 左下

  modelData.vertices.push_back({.position = {1.0f, 1.0f, 0.0f, 1.0f},
                                .texcoord = {1.0f, 0.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 右上

  modelData.vertices.push_back({.position = {1.0f, -1.0f, 0.0f, 1.0f},
                                .texcoord = {1.0f, 1.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 右下

  vertexCount_ = static_cast<uint32_t>(modelData.vertices.size());
  assert(vertexCount_ > 0);

  // ==================
  // 頂点バッファ生成 ＆ 転送
  // ==================
  vbResource_ =
      CreateBufferResource(device_.Get(), sizeof(VertexData) * vertexCount_, L"Particle::vbResource_");

  VertexData *vbMapped = nullptr;
  vbResource_->Map(0, nullptr, reinterpret_cast<void **>(&vbMapped));
  std::memcpy(vbMapped, modelData.vertices.data(),
              sizeof(VertexData) * vertexCount_);
  vbResource_->Unmap(0, nullptr);

  vbView_.BufferLocation = vbResource_->GetGPUVirtualAddress();
  vbView_.StrideInBytes = sizeof(VertexData);
  vbView_.SizeInBytes = sizeof(VertexData) * vertexCount_;

  // ==================
  // Instancing 用 StructuredBuffer
  //   - ParticleForGPU を kNumMaxInstance 個分確保
  //   - CPU から常に Map して直接書き込む
  // ==================
  instanceBufferId_ = sbMgr_->Create(sizeof(ParticleForGPU), kNumMaxInstance);
  instancingData_ =
      static_cast<ParticleForGPU *>(sbMgr_->Map(instanceBufferId_));
  instanceSrv_ = sbMgr_->GetSrv(instanceBufferId_);

  // ==================
  // Material 用定数バッファ (b0)
  // ==================
  cbMat_ = CreateBufferResource(device_.Get(), Align256(sizeof(SpriteMaterial)), L"Particle::cbMat_");
  cbMat_->Map(0, nullptr, reinterpret_cast<void **>(&cbMatMapped_));
  *cbMatMapped_ = SpriteMaterial{}; // 一旦 0 クリア
  cbMatMapped_->color = {1, 1, 1, 1};
  cbMatMapped_->uvTransform = MakeIdentity4x4();
  cbMat_->Unmap(0, nullptr);
  cbMatMapped_ = nullptr;

  // ==================
  // テクスチャ読み込み
  // ==================
  int texHandle = RC::LoadTex(GetTexturePath(), true);
  textureSrv_ = RC::GetSrv(texHandle);

  // ==================
  // パーティクル配列の初期生成
  // ==================
  particles.clear();
  for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
    particles.push_back(
        MakeNewParticle(randomEngine, emitter_.transform.translation));
  }

  // =================
  // エミッタ初期化
  // =================
  emitter_.transform.scale = {1.0f, 1.0f, 1.0f};
  emitter_.transform.rotation = {0.0f, 0.0f, 0.0f};
  emitter_.transform.translation = {0.0f, 0.0f, 0.0f};
  emitter_.count = 3;
  emitter_.frequency = 0.5f;
  emitter_.frequencyTime = 0.0f;

  // ==================
  // 重力フィールド初期化
  // ==================
  accelerationField_.acceleration = {0.01f, 0.0f, 0.0f}; // 強めの右向き重力
  accelerationField_.area.min = {-8.0f, -8.0f, -1.0f};
  accelerationField_.area.max = {8.0f, 8.0f, 1.0f};
}

void Particle::Finalize() {

  particles.clear();

  // ==================
  // GPUリソース破棄
  // ==================
  if (sbMgr_ && instanceBufferId_ >= 0) {
    sbMgr_->Destroy(instanceBufferId_);
    instanceBufferId_ = -1;
    instancingData_ = nullptr;
    instanceSrv_ = {};
  }

  if (vbResource_) {
    vbResource_.Reset();
  }

  if (cbMat_) {
    cbMat_.Reset();
    cbMatMapped_ = nullptr;
  }

  vertexCount_ = 0;
  numInstance = 0;
  device_.Reset();
  sbMgr_ = nullptr;
}

void Particle::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  if (!instancingData_) {
    return;
  }

  // ==================
  // ビルボード用の回転行列を作成
  // ==================
  Matrix4x4 cameraWorld = Inverse(view);

  Matrix4x4 billboardMatrix = MakeIdentity4x4();

  if (useBillboard_) {
    // 右方向
    billboardMatrix.m[0][0] = cameraWorld.m[0][0];
    billboardMatrix.m[0][1] = cameraWorld.m[0][1];
    billboardMatrix.m[0][2] = cameraWorld.m[0][2];

    // 上方向
    billboardMatrix.m[1][0] = cameraWorld.m[1][0];
    billboardMatrix.m[1][1] = cameraWorld.m[1][1];
    billboardMatrix.m[1][2] = cameraWorld.m[1][2];

    // 前方向（Z軸）はカメラに正面を向けるために反転しておく
    billboardMatrix.m[2][0] = -cameraWorld.m[2][0];
    billboardMatrix.m[2][1] = -cameraWorld.m[2][1];
    billboardMatrix.m[2][2] = -cameraWorld.m[2][2];

    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;
    billboardMatrix.m[3][3] = 1.0f;
  }

  if (enableUpdate_ && useEmitterAutoSpawn_) {
    emitter_.frequencyTime += deltaTime;
    if (emitter_.frequencyTime >= emitter_.frequency) {
      // 周期が来たらパーティクルを追加
      particles.splice(particles.end(), Emit(emitter_, randomEngine));
      TrimToMax_();
      emitter_.frequencyTime -= emitter_.frequency;
    }
  }

  // ==================
  // 生きているパーティクルだけ更新して
  // GPU用の instancing バッファに詰める
  //   ※ CPU側は std::list なので、ここで寿命切れを erase していく
  // ==================
  numInstance = 0;

  auto it = particles.begin();
  while (it != particles.end()) {
    ParticleData &p = *it;

    if (p.lifeTime <= p.currentTime) {
      it = particles.erase(it);
      continue;
    }

    if (enableUpdate_) {
      UpdateOneParticle(p, deltaTime);
    }

    // numInstance が上限でも it は必ず進める
    if (numInstance < kNumMaxInstance) {

      const RC::Vector3 &s = p.transform.scale;
      const RC::Vector3 &r = p.transform.rotation;
      const RC::Vector3 &t = p.transform.translation;

      Matrix4x4 world = BuildWorldMatrix(p, billboardMatrix);

      ParticleForGPU wvp{};
      wvp.World = world;
      wvp.WVP = Multiply(world, Multiply(view, proj));
      wvp.color = p.color;

      float alpha = ComputeAlpha(p);
      wvp.color.w = p.color.w * alpha;

      instancingData_[numInstance] = wvp;
      ++numInstance;
    }

    ++it;
  }
}

void Particle::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (!visible_) {
    return;
  }

  if (numInstance == 0) {
    return;
  }

  if (!vbResource_ || instanceSrv_.ptr == 0 || vertexCount_ == 0) {
    return;
  }

  BlendMode prevBlend = RC::GetBlendMode();
  RC::SetBlendMode(blendMode_);

  // ==================
  // 使用するパイプラインステートを決定
  // ==================
  GraphicsPipeline *pso = nullptr;
  if (ctx.pipelineManager) {
    const BlendMode mode = RC::GetBlendMode();
    pso = ctx.pipelineManager->Get(PipelineManager::MakeKey("particle", mode));
    if (!pso && mode != kBlendModeNormal) {
      pso = ctx.pipelineManager->Get(
          PipelineManager::MakeKey("particle", kBlendModeNormal));
    }
  }

  if (!pso) {
    RC::SetBlendMode(prevBlend);
    return;
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());

  // ==================
  // IA 設定（板ポリの頂点バッファ）
  // ==================
  cl->IASetVertexBuffers(0, 1, &vbView_);
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // ==================
  // ルートシグネチャへ各種リソースをバインド
  //   0: Material (b0, PS)
  //   1: インスタンス用 StructuredBuffer (t0, VS)
  //   2: テクスチャ (t1, PS)
  // ==================
  if (cbMat_) {
    cl->SetGraphicsRootConstantBufferView(0, cbMat_->GetGPUVirtualAddress());
  }

  cl->SetGraphicsRootDescriptorTable(1, instanceSrv_);

  if (textureSrv_.ptr != 0) {
    cl->SetGraphicsRootDescriptorTable(2, textureSrv_);
  }

  // ==================
  // インスタンシング描画
  // ==================

  cl->DrawInstanced(vertexCount_, numInstance, 0, 0);

  RC::SetBlendMode(prevBlend);
}

void Particle::DrawImGui() {

#ifdef RC_ENABLE_IMGUI
  if (ImGui::TreeNode("Particle")) {

    // ==================
    // 基本フラグ
    // ==================
    ImGui::Checkbox("Visible", &visible_);
    ImGui::Checkbox("Enable Update", &enableUpdate_);
    ImGui::Checkbox("Use Billboard", &useBillboard_);
    ImGui::Checkbox("Use AccelerationField", &useAccelerationField_);

    {
      static const char *kBlendNames[] = {
          "None", "Normal", "Add", "Subtract", "Multiply", "Screen",
      };

      int current = static_cast<int>(blendMode_);
      if (ImGui::Combo("Blend Mode", &current, kBlendNames,
                       IM_ARRAYSIZE(kBlendNames))) {
        blendMode_ = static_cast<BlendMode>(current);
      }
    }

    ImGui::Text("Count : %d", static_cast<int>(particles.size()));

    if (ImGui::Button("Add Particle (3)")) {
      AddParticlesFromEmitter();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
      particles.clear();
    }

    if (ImGui::Button("Respawn All Particles (Max)")) {
      particles.clear();
      for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
        particles.push_back(
            MakeNewParticle(randomEngine, emitter_.transform.translation));
      }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Emitter");

    ImGui::DragFloat3("EmitterTranslate", &emitter_.transform.translation.x,
                      0.01f, -100.0f, 100.0f);

    // ==================
    // 全パーティクル一括操作
    // ==================
    if (ImGui::TreeNode("All Particles")) {

      // 上書き用の値
      static RC::Vector3 allPos{0.0f, 0.0f, 0.0f};
      static RC::Vector3 allRot{0.0f, 0.0f, 0.0f};
      static RC::Vector3 allScale{1.0f, 1.0f, 1.0f};
      static RC::Vector3 allVel{0.0f, 0.0f, 0.0f};

      ImGui::TextUnformatted("↓この値で全部を上書き");
      ImGui::DragFloat3("All Position", &allPos.x, 0.01f);
      ImGui::DragFloat3("All Rotation", &allRot.x, 0.01f);
      ImGui::DragFloat3("All Scale", &allScale.x, 0.01f);
      ImGui::DragFloat3("All Velocity", &allVel.x, 0.001f);

      if (ImGui::Button("Apply To All")) {
        for (auto &p : particles) {
          p.transform.translation = allPos;
          p.transform.rotation = allRot;
          p.transform.scale = allScale;
          p.velocity = allVel;
        }
      }

      ImGui::Separator();

      // 位置だけオフセット加算したいとき用
      static RC::Vector3 addPos{0.0f, 0.0f, 0.0f};
      ImGui::DragFloat3("Add Position", &addPos.x, 0.01f);
      if (ImGui::Button("Offset All")) {
        for (auto &p : particles) {
          p.transform.translation = Add(p.transform.translation, addPos);
        }
      }

      // 回転だけオフセット加算したいとき用
      static RC::Vector3 addRot{0.0f, 0.0f, 0.0f};
      ImGui::DragFloat3("Add Rotation", &addRot.x, 0.01f);
      if (ImGui::Button("Offset Rotation All")) {
        for (auto &p : particles) {
          p.transform.rotation = Add(p.transform.rotation, addRot);
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Reset Offset")) {
        addPos = {0.0f, 0.0f, 0.0f};
        addRot = {0.0f, 0.0f, 0.0f};
      }

      ImGui::Separator();

      if (ImGui::Button("Reset All Transform/Velocity")) {
        for (auto &p : particles) {
          p.transform.translation = {0.0f, 0.0f, 0.0f};
          p.transform.rotation = {0.0f, 0.0f, 0.0f};
          p.transform.scale = {1.0f, 1.0f, 1.0f};
          p.velocity = {0.0f, 0.0f, 0.0f};
        }
      }

      ImGui::Text("Z軸回転");
      static float zRot = 0.0f;
      ImGui::DragFloat("##Z Rotation", &zRot, 0.01f);
      for (auto &p : particles) {
        p.transform.rotation.z = zRot;
      }

      ImGui::TreePop();
    }

    // ==================
    // UV Transform 一括（ここはもともとのまま）
    // ==================
    if (ImGui::TreeNode("UV Transform")) {
      bool uvChanged = false;

      uvChanged |=
          ImGui::DragFloat2("UV Scale", &uvScale_.x, 0.01f, 0.0f, 10.0f);
      uvChanged |= ImGui::DragFloat2("UV Translate", &uvTranslate_.x, 0.01f);
      uvChanged |= ImGui::DragFloat("UV Rotate", &uvRotate_, 0.01f);

      if (uvChanged && cbMat_) {
        SpriteMaterial *mapped = nullptr;
        if (SUCCEEDED(
                cbMat_->Map(0, nullptr, reinterpret_cast<void **>(&mapped)))) {
          RC::Vector3 s{uvScale_.x, uvScale_.y, 1.0f};
          RC::Vector3 r{0.0f, 0.0f, uvRotate_};
          RC::Vector3 t{uvTranslate_.x, uvTranslate_.y, 0.0f};
          mapped->uvTransform = MakeAffineMatrix(s, r, t);
          cbMat_->Unmap(0, nullptr);
        }
      }

      ImGui::TreePop();
    }

    // ==================
    // 各パーティクル個別編集（list をインデックス付きで表示）
    // ==================
    if (ImGui::TreeNode("Particles")) {
      int i = 0;
      for (auto &p : particles) {
        char label[32];
        std::snprintf(label, sizeof(label), "Particle %d", i++);

        if (ImGui::TreeNode(label)) {
          ImGui::DragFloat3("Translation", &p.transform.translation.x, 0.01f);
          ImGui::DragFloat3("Rotation", &p.transform.rotation.x, 0.01f);
          ImGui::DragFloat3("Scale", &p.transform.scale.x, 0.01f);
          ImGui::DragFloat3("Velocity", &p.velocity.x, 0.001f);

          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }

    ImGui::TreePop();
  }

#endif // _DEBUG
}

ParticleData Particle::MakeNewParticle(std::mt19937 &randomEngine,
                                       const Vector3 &translate) {

  ParticleData particle{};
  InitParticleCore(particle, randomEngine, translate);
  return particle;
}

std::list<ParticleData> Particle::Emit(const Emitter &emitter,
                                       std::mt19937 &randomEngine) {
  std::list<ParticleData> newParticles;
  for (uint32_t count = 0; count < emitter.count; ++count) {
    newParticles.push_back(
        MakeNewParticle(randomEngine, emitter.transform.translation));
  }
  return newParticles;
}

const char *Particle::GetTexturePath() const {
  // デフォルトのテクスチャ
  return "Resources/Particle/circle.png";
}

void Particle::InitParticleCore(ParticleData &particle,
                                std::mt19937 &randomEngine,
                                const Vector3 &translate) {
  // ==================
  // 1個分のパーティクルをランダム初期化（今までの実装をそのまま移植）
  // ==================
  std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};

  particle.transform.scale = {1.0f, 1.0f, 1.0f};
  particle.transform.rotation = {0.0f, 0.0f, 0.0f};
  particle.transform.translation = {
      distribution(randomEngine),
      distribution(randomEngine),
      distribution(randomEngine),
  };

  // エミッタ位置をオフセット
  particle.transform.translation =
      Add(translate, particle.transform.translation);

  particle.velocity = {distribution(randomEngine) * 0.01f,
                       distribution(randomEngine) * 0.01f,
                       distribution(randomEngine) * 0.01f};

  std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
  particle.color = {distColor(randomEngine), distColor(randomEngine),
                    distColor(randomEngine), 1.0f};

  std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
  particle.lifeTime = distTime(randomEngine);
  particle.currentTime = 0.0f;
}

Matrix4x4 Particle::BuildWorldMatrix(const ParticleData &p,
                                     const Matrix4x4 &billboardMatrix) const {
  const Vector3 &s = p.transform.scale;
  const Vector3 &r = p.transform.rotation;
  const Vector3 &t = p.transform.translation;

  if (useBillboard_) {
    Matrix4x4 scaleMat = MakeScaleMatrix(s);
    Matrix4x4 rotateZMat = MakeRotateMatrix(Z, r.z);
    Matrix4x4 translateMat = MakeTranslateMatrix(t);

    Matrix4x4 sr = Multiply(scaleMat, rotateZMat);
    Matrix4x4 srb = Multiply(sr, billboardMatrix);
    return Multiply(srb, translateMat);
  }
  return MakeAffineMatrix(s, r, t);
}

float Particle::ComputeAlpha(const ParticleData &p) const {
  if (p.lifeTime <= 0.0f) {
    return 0.0f;
  }
  float t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);
  return 1.0f - t;
}

void Particle::UpdateOneParticle(ParticleData &p, float dt) {
  // 位置・時間更新
  p.transform += p.velocity;
  p.currentTime += dt;

  // Z軸まわりに1秒で1回転させる
  float spinSpeed = 2.0f * std::numbers::pi_v<float>; // 2π rad/sec = 1回転/秒
  p.transform.rotation.z += spinSpeed * dt;

  // 加速度フィールドが有効なら速度に反映
  if (IsCollision(accelerationField_.area, p.transform.translation) &&
      useAccelerationField_) {
    p.velocity = Add(p.velocity, Multiply(accelerationField_.acceleration, dt));
  }
}

void Particle::TrimToMax_() {
  // 「同時に存在できるパーティクル数」を超えたら古いのから落とす
  while (particles.size() > kNumMaxInstance) {
    particles.pop_front();
  }
}

bool Particle::IsCollision(const AABB &aabb, const Vector3 &point) {
  return (point.x >= aabb.min.x && point.x <= aabb.max.x) &&
         (point.y >= aabb.min.y && point.y <= aabb.max.y) &&
         (point.z >= aabb.min.z && point.z <= aabb.max.z);
}

// ==============================
// キー操作用ユーティリティ
// ==============================

void Particle::SetEnableUpdate(bool enable) { enableUpdate_ = enable; }

void Particle::ToggleEnableUpdate() { enableUpdate_ = !enableUpdate_; }

void Particle::SetUseAccelerationField(bool enable) {
  useAccelerationField_ = enable;
}

void Particle::ToggleUseAccelerationField() {
  useAccelerationField_ = !useAccelerationField_;
}

void Particle::RespawnAllMax() {
  particles.clear();
  for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
    particles.push_back(
        MakeNewParticle(randomEngine, emitter_.transform.translation));
  }
}

void Particle::ClearAll() { particles.clear(); }

void Particle::MoveEmitter(const Vector3 &delta) {
  emitter_.transform.translation = Add(emitter_.transform.translation, delta);
}

void Particle::AddParticlesFromEmitter() {
  particles.splice(particles.end(), Emit(emitter_, randomEngine));
  TrimToMax_();
}

void Particle::SetEmitterAutoSpawn(bool enable) {
  useEmitterAutoSpawn_ = enable;
}

} // namespace RC
