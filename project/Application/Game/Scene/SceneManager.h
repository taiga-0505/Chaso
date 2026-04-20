#pragma once
#include "Fade/Fade.h"
#include "Scene.h"
#include <memory>
#include <string>
#include <unordered_map>

// 前方宣言
class ISceneState;

class Scene::SceneManager {
public:
  SceneManager();
  ~SceneManager();

  // フェード用オブジェクト
  std::unique_ptr<Fade> fade_ = nullptr;
  static inline const float kFadeTime = 1.0f;

public:
  // 初期化・終了処理
  void Init(SceneContext &ctx);
  void Term();

  // シーン登録
  void Register(std::unique_ptr<Scene> scene);

  // 次のフレームで切り替えたいシーン名を予約
  void RequestChange(const std::string &name);

  // 即時切替
  void ChangeImmediately(const std::string &name, SceneContext &ctx);

  void Update(SceneContext &ctx);
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  // 状態遷移
  void ChangeState(std::unique_ptr<ISceneState> newState);

  const std::string &CurrentName() const { return currentName_; }

private:
  Scene *get_(const std::string &name);

private:
  std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
  Scene *current_ = nullptr;
  std::string currentName_;
  std::string requested_;

  // 現在の状態
  std::unique_ptr<ISceneState> state_;

  // 内部アクセス用
  friend class NormalState;
  friend class FadeOutState;
  friend class LoadingState;
  friend class FadeInState;
};
