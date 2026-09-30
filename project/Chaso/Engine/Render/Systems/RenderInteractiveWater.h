#pragma once
#include "Math/MathTypes.h"
#include <d3d12.h>
#include <vector>

namespace RC {

struct WaveSource {
  RC::Vector2 uv;
  float radius;
  float strength;
};

/// @brief 波紋ハイトマップが覆うワールドの一辺（m）。UV = xz / この値 + 0.5
/// @details Water.VS.hlsl / Water.PS.hlsl の waveUV 計算と同じ値。変えるときはシェーダー側も揃える。
inline constexpr float kInteractiveWaterWorldSize = 100.0f;

/// @brief インタラクティブな波紋シミュレーションを初期化します
void InitInteractiveWater();

/// @brief シミュレーションリソースを破棄します
void TermInteractiveWater();

/// @brief 波源を追加します（毎フレームの Update 呼び出し時に消費されます）
void AddWaveSource(const WaveSource& source);

/// @brief ワールド座標を指定して波源を追加します
/// @param worldX ワールド X（m）
/// @param worldZ ワールド Z（m）
/// @param radius 半径（UV 空間。0.01 = 1m）。既存の呼び出し側と同じ単位
/// @param strength 高さの加算値（負で押し下げ）
/// @return ハイトマップの範囲外で捨てられたら false
bool AddWaveSourceAtWorld(float worldX, float worldZ, float radius, float strength);

/// @brief コンピュートシェーダーを実行し、波のシミュレーションを1ステップ進めます
void UpdateInteractiveWater();

/// @brief 最新のハイトマップの GPU SRV ハンドルを取得します
D3D12_GPU_DESCRIPTOR_HANDLE GetInteractiveWaterHeightMap();

// ---------------------------------------------------------------------------
// CPU 読み戻し（任意）
// ---------------------------------------------------------------------------
// 浮遊物を波紋に反応させたいシーン（タイトルなど）だけが有効にする。
// 有効中は毎フレーム GPU→READBACK バッファへ 256KB コピーし、フェンス完了後に
// CPU 側の配列へ写す（1〜2 フレーム遅れ。GPU 待ちはしない）。無効なら一切コストは掛からない。
// 有効にした側が OnDestroy 等で必ず無効に戻すこと。

/// @brief ハイトマップの CPU 読み戻しを有効／無効にします
void SetInteractiveWaterReadback(bool enabled);

/// @brief CPU 読み戻しが有効か
bool IsInteractiveWaterReadbackEnabled();

/// @brief 波紋の高さ（m）を取得します。読み戻しが無効／未着なら 0
/// @details Water.VS.hlsl と同じ写像（xz/100+0.5）とサンプラ（linear, clamp）で読む。
float SampleInteractiveWaterHeight(float worldX, float worldZ);

/// @brief 波紋の高さと法線を取得します
/// @param outHeight 波紋の高さ（m）
/// @param outNormal 波紋だけの法線（Water.VS.hlsl の interactiveNormal と同じ有限差分）
/// @return 読み戻しが無効／未着なら false（outHeight=0, outNormal=(0,1,0)）
bool SampleInteractiveWater(float worldX, float worldZ, float& outHeight, Vector3& outNormal);

} // namespace RC
