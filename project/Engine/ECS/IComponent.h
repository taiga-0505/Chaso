#pragma once

/// @brief コンポーネントの基底インターフェース
/// Entity にアタッチされるすべてのコンポーネントはこのクラスを継承する必要があります。
class IComponent {
public:
  virtual ~IComponent() = default;

  /// @brief 毎フレームの更新処理
  /// @param deltaTime 前フレームからの経過時間（秒）
  virtual void Update(float deltaTime) { (void)deltaTime; }

  /// @brief コンポーネントの有効状態を取得
  /// @return 有効なら true
  bool IsEnabled() const { return enabled_; }
  
  /// @brief コンポーネントの有効状態を設定
  /// @param enabled 設定する状態
  void SetEnabled(bool enabled) { enabled_ = enabled; }

protected:
  bool enabled_ = true; ///< 有効フラグ（false の場合は Update が呼ばれない）
};
