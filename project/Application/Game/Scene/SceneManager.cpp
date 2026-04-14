#include "SceneManager.h"
#include "Scene.h"

class FadeOutState;
class FadeInState;
class NormalState;

// =================================================================
// 状態インタフェース
// =================================================================
class ISceneState {
public:
  virtual ~ISceneState() = default;
  virtual void Update(Scene::SceneManager &sm, SceneContext &ctx) = 0;
  virtual void Render(Scene::SceneManager &sm, SceneContext &ctx,
                      ID3D12GraphicsCommandList *cl) = 0;
};

// =================================================================
// 各状態クラスの定義
// =================================================================

class NormalState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
};

class FadeOutState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
};

class FadeInState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
};

// =================================================================
// NormalState 実装
// =================================================================
void NormalState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  if (!sm.requested_.empty()) {
    sm.fade_->Start(Fade::Status::FadeOut, sm.kFadeTime);
    sm.ChangeState(std::make_unique<FadeOutState>());
  } else if (sm.current_) {
    sm.current_->Update(sm, ctx);
  }
}
void NormalState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                         ID3D12GraphicsCommandList *cl) {
  if (sm.current_) {
    sm.current_->Render(ctx, cl);
  }
}

// =================================================================
// FadeOutState 実装
// =================================================================
void FadeOutState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  if (sm.fade_->IsFinished()) {
    sm.ChangeImmediately(sm.requested_, ctx);
    sm.fade_->Start(Fade::Status::FadeIn, sm.kFadeTime);
    sm.ChangeState(std::make_unique<FadeInState>());
    if (sm.current_) {
      sm.current_->Update(sm, ctx);
    }
  }
}
void FadeOutState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                          ID3D12GraphicsCommandList *cl) {
  if (sm.current_) {
    sm.current_->Render(ctx, cl);
  }
  sm.fade_->Draw(cl);
}

// =================================================================
// FadeInState 実装
// =================================================================
void FadeInState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  if (sm.fade_->IsFinished()) {
    sm.fade_->Stop();
    sm.ChangeState(std::make_unique<NormalState>());
  }
}
void FadeInState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                         ID3D12GraphicsCommandList *cl) {
  if (sm.current_) {
    sm.current_->Render(ctx, cl);
  }
  sm.fade_->Draw(cl);
}

// =================================================================
// SceneManager 実装
// =================================================================

Scene::SceneManager::SceneManager() = default;
Scene::SceneManager::~SceneManager() = default;

void Scene::SceneManager::Init(SceneContext &ctx) {
  float width = float(ctx.app->width);
  float height = float(ctx.app->height);

  // Fadeコンポーネントを初期化
  fade_ = std::make_unique<Fade>();
  fade_->Init(ctx, width, height);

  // 初期状態をセット
  ChangeState(std::make_unique<NormalState>());
}

void Scene::SceneManager::Term() {
  if (fade_) {
    fade_.reset();
  }
  scenes_.clear();
  current_ = nullptr;
  currentName_.clear();
  requested_.clear();
  state_.reset();
}

void Scene::SceneManager::Register(std::unique_ptr<Scene> scene) {
  const std::string key = scene->Name();
  scenes_[key] = std::move(scene);
}

void Scene::SceneManager::RequestChange(const std::string &name) {
  requested_ = name;
}

void Scene::SceneManager::ChangeImmediately(const std::string &name,
                                            SceneContext &ctx) {
  if (current_) {
    current_->OnExit(ctx);
  }
  current_ = get_(name);
  if (current_) {
    current_->OnEnter(ctx);
  }
  currentName_ = name;
  requested_.clear();
}

void Scene::SceneManager::Update(SceneContext &ctx) {
  // 常にFadeの更新は行う
  if (fade_) {
    fade_->Update();
  }

  // 現在の状態に更新処理を委譲
  if (state_) {
    state_->Update(*this, ctx);
  }
}

void Scene::SceneManager::Render(SceneContext &ctx,
                                 ID3D12GraphicsCommandList *cl) {
  // 現在の状態に描画処理を委譲
  if (state_) {
    state_->Render(*this, ctx, cl);
  }
}

void Scene::SceneManager::ChangeState(std::unique_ptr<ISceneState> newState) {
  state_ = std::move(newState);
}

Scene *Scene::SceneManager::get_(const std::string &name) {
  auto it = scenes_.find(name);
  return (it == scenes_.end()) ? nullptr : it->second.get();
}
