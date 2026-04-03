#include "CircleParticle.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "Input/Input.h"

namespace RC {

void CircleParticle::Initialize(SceneContext &ctx) {
  // 共通初期化
  Particle::Initialize(ctx);

  // 霊魂用のエミッタ設定
  Emitter &e = EmitterRef();

  // 1回の Emit で出す個数
  //   少なめ + 大きめのパーティクルで「一つの魂」に見せる
  e.count = 20;

  // Emit 間隔（小さいほど連続で出る）
  e.frequency = 0.05f; // 約 20fps ごとにちょっとずつ補充

  // 火力デフォルト
  firePower_ = 1.0f;
}

// ==============================
// A / D 入力付きの Update
// ==============================
void CircleParticle::UpdateWithInput(SceneContext &ctx, const RC::Matrix4x4 &view,
                                   const RC::Matrix4x4 &proj) {
  if (ctx.input) {
    if (ctx.input->IsKeyPressed(DIK_A)) {
      // 火力減少（火が消える方向へ）
      AddCirclePower(-0.01f);
    }
    if (ctx.input->IsKeyPressed(DIK_D)) {
      // 火力上昇（明るく＆青く）
      AddCirclePower(+0.01f);
    }
  }

  // 通常のパーティクル更新
  Particle::Update(view, proj);
}

// ==============================
// 火力操作
// ==============================
void CircleParticle::AddCirclePower(float delta) {
  firePower_ = std::clamp(firePower_ + delta, firePowerMin_, firePowerMax_);
}

void CircleParticle::SetCirclePower(float v) {
  firePower_ = std::clamp(v, firePowerMin_, firePowerMax_);
}

// ==============================
// Emit：火力に応じて発生数を変える
// ==============================
std::vector<ParticleData> CircleParticle::Emit(const Emitter &emitter,
                                             std::mt19937 &randomEngine) {
  std::vector<ParticleData> newParticles;

  float power = std::clamp(firePower_, firePowerMin_, firePowerMax_);

  // ほぼ 0 なら何も出さない
  if (power <= 0.01f) {
    return newParticles;
  }

  // 火力に応じて 0.3〜1.8 倍くらいの発生数
  const int baseCount = static_cast<int>(emitter.count);
  float rate = 0.3f + power * 0.75f; // power=1 → ~1.05, power=2 → ~1.8
  int spawnCount = static_cast<int>(std::round(baseCount * rate));
  spawnCount = (std::max)(spawnCount, 1);

  newParticles.reserve(spawnCount);
  for (int i = 0; i < spawnCount; ++i) {
    ParticleData p{};
    InitParticleCore(p, randomEngine, EmitterRef().transform.translation);
    newParticles.push_back(p);
  }

  return newParticles;
}

// ==============================
// 1個分の初期化
// ==============================
void CircleParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                    const Vector3 &emitterPos) {

  using std::numbers::pi_v;

  float power = std::clamp(firePower_, firePowerMin_, firePowerMax_);

  // 円形＋少し縦長のボリュームで「魂のかたまり」っぽい形を作る
  std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * pi_v<float>);
  std::uniform_real_distribution<float> distRadius(0.0f, 0.6f);
  std::uniform_real_distribution<float> distHeight(0.0f, 0.8f);

  float angle = distAngle(rng);
  float radius = distRadius(rng);

  float offsetX = std::cos(angle) * radius * 0.4f; // 横は狭め
  float offsetZ = std::sin(angle) * radius * 0.4f;
  float offsetY = distHeight(rng); // 上方向に少し伸ばす

  // 基本スケール
  std::uniform_real_distribution<float> distScaleJitter(0.7f, 1.2f);
  float baseScale = 0.35f;
  float powerScale = 0.6f + 0.4f * (std::min)(power, 1.0f); // 0.6〜1.0
  float s = baseScale * powerScale * distScaleJitter(rng);

  // 少し縦長にする
  p.transform.scale = {s, s * 1.4f, s};
  p.transform.rotation = {0.0f, 0.0f, 0.0f};
  p.transform.translation = {
      emitterPos.x + offsetX,
      emitterPos.y + offsetY,
      emitterPos.z + offsetZ,
  };

  // 横揺れ用のランダム位相を rotation.y に仕込んでおく
  std::uniform_real_distribution<float> distPhase(0.0f, 2.0f * pi_v<float>);
  p.transform.rotation.y = distPhase(rng);

  // ゆっくり上に漂う感じの速度
  std::uniform_real_distribution<float> distVelSide(-0.005f, 0.005f);
  std::uniform_real_distribution<float> distVelUp(0.01f, 0.03f);

  p.velocity = {
      distVelSide(rng),
      distVelUp(rng),
      distVelSide(rng),
  };

  // ベースは青白い色
  p.color = {0.9f, 0.97f, 1.0f, 0.7f};

  // 寿命：少し長め
  std::uniform_real_distribution<float> distLife(1.2f, 2.0f);
  float life = distLife(rng);
  float lifeFactor = 0.6f + 0.4f * (std::min)(power, 1.0f); // 0.6〜1.0
  p.lifeTime = life * lifeFactor;
  p.currentTime = 0.0f;
}

// ==============================
// 1個分の更新
// ==============================
void CircleParticle::UpdateOneParticle(ParticleData &p, float dt) {
  // まずは基底クラスの標準挙動で
  Particle::UpdateOneParticle(p, dt);

  float t = 0.0f;
  if (p.lifeTime > 0.0f) {
    t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);
  }

  float power = std::clamp(firePower_, firePowerMin_, firePowerMax_);
  const Vector3 emitterPos = EmitterRef().transform.translation;

  // ==========================
  // その場に留まるように、縦方向をある程度の範囲に制限
  // ==========================
  float dy = p.transform.translation.y - emitterPos.y;
  const float maxUp = 0.8f;   // どこまで上に行ってよいか
  const float maxDown = 0.0f; // 下にはほぼ行かせない

  if (dy > maxUp) {
    p.transform.translation.y = emitterPos.y + maxUp;
    if (p.velocity.y > 0.0f) {
      p.velocity.y *= 0.0f; // これ以上上に行かない
    }
  } else if (dy < maxDown) {
    p.transform.translation.y = emitterPos.y + maxDown;
    if (p.velocity.y < 0.0f) {
      p.velocity.y *= 0.0f;
    }
  }

  // 横方向はだんだん減衰させて、広がりすぎないようにする
  p.velocity.x *= 0.96f;
  p.velocity.z *= 0.96f;

  // ==========================
  // ふわふわ横に揺れる動き
  // ==========================
  float phase = p.transform.rotation.y; // Init で仕込んだ乱数位相
  float wobbleSpeed = 3.0f + power * 2.0f;
  float wobbleRadius =
      (0.05f + 0.08f * power) * (1.0f - t); // 最初は大きく、消えるときに小さく

  float wobbleX = std::sin(p.currentTime * wobbleSpeed + phase) * wobbleRadius;
  float wobbleZ = std::cos(p.currentTime * wobbleSpeed + phase) * wobbleRadius;

  p.transform.translation.x += wobbleX;
  p.transform.translation.z += wobbleZ;

  // ==========================
  // 色と透明度
  //   ・火力 0〜1 → 暖色寄りの魂
  //   ・火力 1〜2 → 青く強く光る
  // ==========================
  auto lerp = [](float a, float b, float r) { return a * (1.0f - r) + b * r; };

  // 暖かい炎寄りの色
  const float warmR = 1.0f, warmG = 0.8f, warmB = 0.4f;
  // 青白い魂の色
  const float soulStartR = 0.9f, soulStartG = 0.97f, soulStartB = 1.0f;
  const float soulEndR = 0.2f, soulEndG = 0.5f, soulEndB = 1.0f;

  // 時間に応じて魂色を少し青くしていく
  float soulR = lerp(soulStartR, soulEndR, t);
  float soulG = lerp(soulStartG, soulEndG, t);
  float soulB = lerp(soulStartB, soulEndB, t);

  // 火力 0〜1 で暖色→青白に変化
  float soulRate = std::clamp(power, 0.0f, 1.0f);
  float r = lerp(warmR, soulR, soulRate);
  float g = lerp(warmG, soulG, soulRate);
  float b = lerp(warmB, soulB, soulRate);

  // 火力 1〜2 ではさらに少しだけ青く強調
  if (power > 1.0f) {
    float extra = std::clamp(power - 1.0f, 0.0f, 1.0f);
    r = lerp(r, soulEndR, extra * 0.7f);
    g = lerp(g, soulEndG, extra * 0.7f);
    b = lerp(b, soulEndB, extra * 0.7f);
  }

  p.color.x = r;
  p.color.y = g;
  p.color.z = b;

  // アルファ：序盤は明るく、だんだんスッと消える
  float alphaBase = 0.35f + 0.4f * (std::min)(power, 1.5f); // 0.35〜0.95 くらい
  float fade = (1.0f - t);
  fade = fade * fade; // 少しだけイージング
  p.color.w = alphaBase * fade;
}

} // namespace RC
