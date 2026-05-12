#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

/// @brief 平行光源(Directional Light)を制御するクラス
/// 光の方向、色、輝度、およびライティングモード（Lambert/HalfLambert）を管理します。
class DirectionalLightSource {
public:
  /// @brief コンストラクタ。デフォルト値を設定します。
  DirectionalLightSource();

  /// @brief ライトの生データ(構造体)を取得
  /// @return DirectionalLight構造体への参照
  DirectionalLight &Data() { return data_; }
  
  /// @brief ライトの生データ(構造体)を取得 (const)
  /// @return DirectionalLight構造体へのconst参照
  const DirectionalLight &Data() const { return data_; }

  /// @brief 有効状態を取得
  /// @return 有効なら true
  bool IsEnabled() const { return enabled_; }
  
  /// @brief 有効状態を設定
  /// @param enabled 有効にするなら true
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  /// @brief GPU転送用のデータを取得
  /// 有効フラグが false の場合は、強度を 0 に設定したコピーを返します。
  /// @return GPU転送用データ
  DirectionalLight DataForGPU() const;

  /// @brief 光の方向を設定
  /// @param dir 方向ベクトル
  /// @param normalize ベクトルを正規化するかどうか
  void SetDirection(const Vector3 &dir, bool normalize = true);
  
  /// @brief ライトの色を設定 (RGB)
  /// @param rgb 色成分
  /// @param alpha 透明度
  void SetColor(const Vector3 &rgb, float alpha = 1.0f);
  
  /// @brief ライトの色を設定 (RGBA)
  /// @param rgba 4成分の色
  void SetColor(const Vector4 &rgba);
  
  /// @brief 輝度を設定
  /// @param intensity 輝度値
  void SetIntensity(float intensity);

  /// @brief ライティングモードを取得
  /// @return モジュール内部の定義値 (0:None, 1:Lambert, 2:HalfLambert)
  int GetLightingMode() const { return lightingMode_; }
  
  /// @brief ライティングモードを設定
  /// @param m モード指定 (0:None, 1:Lambert, 2:HalfLambert)
  void SetLightingMode(int m) { lightingMode_ = m; }

  /// @brief 光沢度(Shininess)を設定
  /// スペキュラ計算に使用します。
  /// @param s 光沢度
  void SetShininess(float s) { shininess_ = s; }
  
  /// @brief 光沢度(Shininess)を取得
  /// @return 光沢度
  float GetShininess() const { return shininess_; }

  /// @brief ImGuiによるパラメータ編集UIを表示
  /// @param name UIに表示するラベル
  void DrawImGui(const char *name = nullptr);

private:
  DirectionalLight data_{}; ///< ライトパラメータ実体
  bool enabled_ = true;      ///< 有効フラグ
  int lightingMode_ = 2;    ///< ライティングモード (デフォルト: HalfLambert)
  float shininess_ = 32.0f; ///< スペキュラ用光沢度
};

} // namespace RC
