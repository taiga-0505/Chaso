#pragma once

// ============================================================================
// Entity
// ----------------------------------------------------------------------------
// 軽量エンティティ。コンポーネントをアタッチするための最小コンテナ。
// テンプレートベースの AddComponent / GetComponent で型安全にアクセスする。
// ============================================================================

#include "IComponent.h"

#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

class Entity {
public:
  /// コンストラクタ（内部IDは自動採番）
  Entity() : id_(NextId()) {}
  explicit Entity(const std::string &name) : id_(NextId()), name_(name) {}

  /// コンポーネントの追加（既に同型があれば上書き）
  template <typename T, typename... Args> T &AddComponent(Args &&...args) {
    static_assert(std::is_base_of_v<IComponent, T>,
                  "T must derive from IComponent");
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    T &ref = *ptr;
    components_[std::type_index(typeid(T))] = std::move(ptr);
    return ref;
  }

  /// コンポーネントの取得（なければ nullptr）
  template <typename T> T *GetComponent() {
    auto it = components_.find(std::type_index(typeid(T)));
    if (it != components_.end()) {
      return static_cast<T *>(it->second.get());
    }
    return nullptr;
  }

  template <typename T> const T *GetComponent() const {
    auto it = components_.find(std::type_index(typeid(T)));
    if (it != components_.end()) {
      return static_cast<const T *>(it->second.get());
    }
    return nullptr;
  }

  /// コンポーネントを持っているか
  template <typename T> bool HasComponent() const {
    return components_.count(std::type_index(typeid(T))) > 0;
  }

  /// コンポーネントの削除
  template <typename T> void RemoveComponent() {
    components_.erase(std::type_index(typeid(T)));
  }

  /// 全コンポーネントの Update を呼ぶ
  void UpdateAll(float deltaTime) {
    for (auto &[type, comp] : components_) {
      if (comp && comp->IsEnabled()) {
        comp->Update(deltaTime);
      }
    }
  }

  // ── アクティブ状態 ──
  bool IsActive() const { return active_; }
  void SetActive(bool active) { active_ = active; }

  // ── 識別 ──
  uint32_t Id() const { return id_; }
  const std::string &Name() const { return name_; }
  void SetName(const std::string &name) { name_ = name; }

private:
  uint32_t id_;
  std::string name_;
  bool active_ = true;

  std::unordered_map<std::type_index, std::unique_ptr<IComponent>> components_;

  static uint32_t NextId() {
    static uint32_t counter = 0;
    return ++counter;
  }
};
