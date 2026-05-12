#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

/// @brief 矩形エリアライト(Rect AreaLight)の操作用クラス
/// CPU側でパラメータを保持し、GPUへ送るためのデータ構造体(AreaLight)を管理します。
class AreaLightSource {
public:
  /// @brief コンストラクタ。デフォルト値を設定します。
  AreaLightSource();

  /// @brief ライトの生データ(構造体)を取得
  /// @return AreaLight構造体への参照
  ::AreaLight &Data() { return data_; }
  
  /// @brief ライトの生データ(構造体)を取得 (const)
  /// @return AreaLight構造体へのconst参照
  const ::AreaLight &Data() const { return data_; }

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

  /// @brief ライトの方向軸(基底)を設定
  /// 面の向きを決定する2軸を指定します。
  /// @param right 右方向ベクトル
  /// @param up 上方向ベクトル
  /// @param normalize ベクトルを正規化するかどうか
  void SetBasis(const Vector3 &right, const Vector3 &up, bool normalize = true);

  /// @brief エリアライトのサイズを設定
  /// @param width 幅（フルサイズ）
  /// @param height 高さ（フルサイズ）
  void SetSize(float width, float height);

  /// @brief エリアライトの半分サイズを設定
  /// @param halfWidth 半幅
  /// @param halfHeight 半高さ
  void SetHalfSize(float halfWidth, float halfHeight) {
    data_.halfWidth = halfWidth;
    data_.halfHeight = halfHeight;
  }

  /// @brief 影響範囲を設定
  /// @param r 距離
  void SetRange(float r) { data_.range = r; }
  
  /// @brief 減衰パラメータを設定
  /// @param d 減衰率
  void SetDecay(float d) { data_.decay = d; }

  /// @brief 両面発光設定
  /// @param b true で両面を発光させる
  void SetTwoSided(bool b) { data_.twoSided = b ? 1u : 0u; }
  
  /// @brief 両面発光か取得
  /// @return true なら両面
  bool IsTwoSided() const { return data_.twoSided != 0; }

  /// @brief ImGuiによるパラメータ編集UIを表示
  /// @param name UIに表示するラベル
  void DrawImGui(const char *name = nullptr);

  /// @brief 有効・無効切り替え
  /// @param enabled true で有効
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  
  /// @brief 有効状態を取得
  /// @return 有効なら true
  bool IsEnabled() const { return enabled_; }
  
  /// @brief 有効・無効を反転させる
  void ToggleEnabled() { enabled_ = !enabled_; }

  /// @brief GPU転送用のデータを取得
  /// enabledフラグの状態を反映（falseなら輝度を0にする等）したコピーを返します。
  /// @return GPU転送用データ
  ::AreaLight DataForGPU() const;

private:
  bool enabled_ = true; ///< 有効フラグ
  ::AreaLight data_{};  ///< ライトパラメータ実体
};

} // namespace RC
