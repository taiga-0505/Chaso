#pragma once

// ============================================================================
// IComponent
// ----------------------------------------------------------------------------
// コンポーネントの基底インターフェース。
// Entity にアタッチされるすべてのコンポーネントはこれを継承する。
// ============================================================================

class IComponent {
public:
  virtual ~IComponent() = default;

  /// 毎フレーム更新（deltaTime は秒）
  virtual void Update(float deltaTime) { (void)deltaTime; }

  /// コンポーネントが有効かどうか
  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

protected:
  bool enabled_ = true;
};
