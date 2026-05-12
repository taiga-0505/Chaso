#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

/// @brief 点光源(Point Light)を制御するクラス
/// 位置、色、輝度、半径、および減衰率を管理します。
class PointLightSource {
public:
  /// @brief コンストラクタ。デフォルト値を設定します。
  PointLightSource();

  /// @brief ライトの生データ(構造体)を取得
  /// @return PointLight構造体への参照
  ::PointLight &Data() { return data_; }
  
  /// @brief ライトの生データ(構造体)を取得 (const)
  /// @return PointLight構造体へのconst参照
  const ::PointLight &Data() const { return data_; }

  /// @brief ライトの位置を設定
  /// @param pos 配置座標
  void SetPosition(const Vector3 &pos) { data_.position = pos; }

  /// @brief ライトの色を設定 (RGB)
  /// @param rgb 色成分
  /// @param alpha 透明度
  void SetColor(const Vector3 &rgb, float alpha = 1.0f) {
    data_.color = {rgb.x, rgb.y, rgb.z, alpha};
  }
  
  /// @brief ライトの色を設定 (RGBA)
  /// @param rgba 4成分の色
  void SetColor(const Vector4 &rgba) { data_.color = rgba; }

  /// @brief 輝度を設定
  /// @param v 輝度値
  void SetIntensity(float v) { data_.intensity = v; }

  /// @brief 影響半径を設定
  /// @param r 半径
  void SetRadius(float r) { data_.radius = r; }

  /// @brief 減衰パラメータを設定
  /// @param d 減衰率
  void SetDecay(float d) { data_.decay = d; }

  /// @brief 有効状態を設定
  /// @param enabled 有効にするなら true
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  
  /// @brief 有効状態を取得
  /// @return 有効なら true
  bool IsEnabled() const { return enabled_; }
  
  /// @brief 有効・無効を反転させる
  void ToggleEnabled() { enabled_ = !enabled_; }

  /// @brief GPU転送用のデータを取得
  /// 有効フラグが false の場合は、強度を 0 に設定したコピーを返します。
  /// @return GPU転送用データ
  ::PointLight DataForGPU() const;

  /// @brief ImGuiによるパラメータ編集UIを表示
  /// @param name UIに表示するラベル
  void DrawImGui(const char *name = nullptr);

private:
  bool enabled_ = true; ///< 有効フラグ
  ::PointLight data_{}; ///< ライトパラメータ実体
};

} // namespace RC
