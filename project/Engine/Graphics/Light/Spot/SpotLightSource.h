#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

/// @brief スポットライト(Spot Light)を制御するクラス
/// 位置、方向、色、輝度、および照射角度（カットオフ）を管理します。
class SpotLightSource {
public:
  /// @brief コンストラクタ。デフォルト値を設定します。
  SpotLightSource();

  /// @brief ライトの生データ(構造体)を取得
  /// @return SpotLight構造体への参照
  ::SpotLight &Data() { return data_; }
  
  /// @brief ライトの生データ(構造体)を取得 (const)
  /// @return SpotLight構造体へのconst参照
  const ::SpotLight &Data() const { return data_; }

  /// @brief ライトの位置を設定
  /// @param pos 配置座標
  void SetPosition(const Vector3 &pos) { data_.position = pos; }

  /// @brief ライトの照射方向を設定
  /// @param dir 方向ベクトル
  void SetDirection(const Vector3 &dir) { data_.direction = dir; }

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

  /// @brief 届く距離(影響範囲)を設定
  /// @param d 距離（0以下で無効）
  void SetDistance(float d) { data_.distance = d; }

  /// @brief 距離減衰の指数を設定
  /// @param d 減衰率（0に近いほど緩やかに減衰）
  void SetDecay(float d) { data_.decay = d; }

  /// @brief 照射角のコサイン値を設定
  /// 外側のカットオフ角度として扱われます。
  /// @param c コサイン値 (例: cos(30度))
  void SetCosAngle(float c) { data_.cosAngle = c; }

  /// @brief 角度（度数法）から照射角を設定
  /// @param deg 角度（度）
  void SetAngleDeg(float deg);
  
  /// @brief 角度（弧度法）から照射角を設定
  /// @param rad 角度（ラジアン）
  void SetAngleRad(float rad);

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
  ::SpotLight DataForGPU() const;

  /// @brief ImGuiによるパラメータ編集UIを表示
  /// @param name UIに表示するラベル
  void DrawImGui(const char *name = nullptr);

private:
  bool enabled_ = true; ///< 有効フラグ
  ::SpotLight data_{};  ///< ライトパラメータ実体
};

} // namespace RC
