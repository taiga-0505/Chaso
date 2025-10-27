#pragma once
#include "Fade/Fade.h"
#include "Scene.h"
#include <memory>
#include <string>
#include <unordered_map>

class Scene::SceneManager {
public:
  // フェード用オブジェクト
  std::unique_ptr<Fade> fade_ = nullptr;
  static inline const float kFadeTime = 1.0f;

  // シーン遷移のステップを管理する状態
  enum class ChangePhase {
    kNone,      // 通常時
    kFadingOut, // フェードアウト実行中
    kChanging,  // シーンの即時切替 (ChangeImmediately) 実行
    kFadingIn,  // フェードイン実行中
  };
  ChangePhase changePhase_ = ChangePhase::kNone;

public:
  // フェードを使用するために初期化
  void Init(SceneContext &ctx) {
    ID3D12Device *device = ctx.core->GetDevice();
    auto &srvHeap = ctx.core->SRV();

    float width = float(ctx.app->width);
    float height = float(ctx.app->height);

    // Fadeコンポーネントを初期化
    fade_ = std::make_unique<Fade>();
    fade_->Init(device, &srvHeap, width, height);
  }

  void Register(std::unique_ptr<Scene> scene) {
    const std::string key = scene->Name();
    // 既存の同名シーンがあれば上書きする
    scenes_[key] = std::move(scene);
  }

  void Term() {
    // フェード破棄（Fade::Termがあるなら呼び、その後reset）
    if (fade_) {
      // fade_->Term(); // 実装があるなら有効化
      fade_.reset();
    }
    // シーン達を全破棄（OnExitを呼ぶ余地がなければresetでOK）
    scenes_.clear();
    current_ = nullptr;
    currentName_.clear();
    requested_.clear();
    changePhase_ = ChangePhase::kNone;
  }

  // 次のフレームで切り替えたいシーン名を予約
  void RequestChange(const std::string &name) { requested_ = name; }

  // 即時切替（初期化でのみ使用）
  void ChangeImmediately(const std::string &name, SceneContext &ctx) {
    if (current_)
      current_->OnExit(ctx);
    current_ = get_(name);
    if (current_)
      current_->OnEnter(ctx);
    currentName_ = name;
    requested_.clear();
  }

  void Update(SceneContext &ctx) {
    // 常にFadeの更新は行う（アニメーション処理）
    fade_->Update();

    switch (changePhase_) {
    case ChangePhase::kNone:
      // 遷移要求があればフェードアウト開始
      if (!requested_.empty()) {
        fade_->Start(Fade::Status::FadeOut, kFadeTime);
        changePhase_ = ChangePhase::kFadingOut;
      } else if (current_) {
        current_->Update(*this, ctx);
      }
      break;

    case ChangePhase::kFadingOut:
      if (fade_->IsFinished()) {
        // フェードアウト完了 -> シーン即時切替
        ChangeImmediately(requested_, ctx);
        requested_.clear();

        // 新しいシーンの表示前に、フェードインを開始
        fade_->Start(Fade::Status::FadeIn, kFadeTime); // 0.5秒でイン
        changePhase_ = ChangePhase::kFadingIn;
      }
      // フェード中はシーンのUpdateはスキップ
      break;

    case ChangePhase::kFadingIn:
      if (fade_->IsFinished()) {
        // フェードイン完了 -> 通常状態に戻る
        fade_->Stop();
        changePhase_ = ChangePhase::kNone;
      }
      // フェード中はシーンのUpdateはスキップ
      break;
    }
  }

  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
    if (current_)
      current_->Render(ctx, cl);

    // フェードが実行中であれば、シーンの上に重ねて描画
    if (changePhase_ == ChangePhase::kFadingOut ||
        changePhase_ == ChangePhase::kFadingIn) {
      fade_->Draw(cl);
    }
  }

  const std::string &CurrentName() const { return currentName_; }

private:
  Scene *get_(const std::string &name) {
    auto it = scenes_.find(name);
    return (it == scenes_.end()) ? nullptr : it->second.get();
  }

  std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
  Scene *current_ = nullptr;
  std::string currentName_;
  std::string requested_;
};
