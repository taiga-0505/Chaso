#pragma once

// ============================================================================
// CaptureMode — 撮影モード
// ----------------------------------------------------------------------------
// 何のために作ったか
// ------------------
// メンター評価では「動画から客観的に確認できない成果は0点」というルールで
// 採点されている。実際、実装は進んでいるのに
//
//   ・PCF を入れたこと自体が動画では証明できない
//   ・岩に当たった波が反射して離れていく瞬間が確認できない
//   ・Title → Game → Result → Title の一周が写っていない
//   ・T-20 / A-05 / D-07〜D-10 / 浮力の数値検証が写っていない
//
// という理由で、3週続けて成果点を落としている。
// つまり足りていないのは実装ではなく「完成を証明する工程」なので、
// 撮って出しの動画がそのまま証拠になる状態を作る。
//
// やること
// --------
// F9 を押すとエディタの他のパネルを全部隠し、ゲーム画面だけを全画面にする。
// ウィンドウごと録画すれば、そのままプレイ動画になる。
// 以前は字幕（証明したい内容・実測値・操作ヒント）を重ねていたが、
// 録画の邪魔になるので外した。計測結果はログへ出す。
//
// ステップは第1週〜第4週の未確認項目を撮影順に並べてある。
// F10 / F8 で前後、F7 でそのステップの仕込み（PCF切替やウェーブ開始など）を実行する。
// ============================================================================

#include <d3d12.h>

class Dx12Core;
class Scene;

/// @class CaptureMode
/// @brief 撮影モード（ゲーム画面の全画面表示）
/// @details 状態は静的に持つ。撮影モードは1つしか無いので実害はない。
class CaptureMode {
public:
  /// @brief 撮影モード中か
  static bool IsActive();

  /// @brief 撮影モードの ON / OFF を切り替える
  static void SetActive(bool active);

  /// @brief ゲーム画面を全画面で敷き、ステップごとの処理（自動切り替え・計測）を進める
  /// @param viewportSrv ゲーム画面のSRV
  /// @param core Dx12Core（アスペクト比の算出に使う）。nullptr 可
  /// @param currentScene 現在のシーン。nullptr 可
  /// @param deltaTime 前フレームからの経過時間（秒）
  /// @note 撮影モード中は EditorManager 側で他のパネルを描かないこと。
  static void Draw(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core *core,
                   Scene *currentScene, float deltaTime);

  /// @brief F9（撮影モードの開始/終了）などのキーを拾う
  /// @details 撮影モードに入っていなくても毎フレーム呼ぶこと。
  /// @return 撮影モードの ON/OFF が切り替わったら true
  static bool HandleHotkeys();

  /// @brief 撮影モードに入るとき、シーンを再生状態にする必要があるか
  /// @details 浮力やウェーブ戦闘は再生中でないと動かないため、
  ///          EditorManager から PlayState を Playing にしてもらう。
  static bool WantsPlaying();

  /// @brief いまマウスがゲーム画面の上にあるか（撮影モード中は常に true）
  /// @details 通常時は Viewport パネルの `ImGui::IsWindowHovered()` が担っている判定。
  ///          撮影モードでは Viewport パネルを描かないので、代わりにこれを
  ///          `EditorManager::isViewportHovered_` へ渡す。
  ///          これが false のあいだは射撃が止まるので、落下高さのスライダを
  ///          ドラッグしているあいだに弾が出ることもない。
  /// @note 値は Draw() の中で更新される（Viewport パネルと同じく 1 フレーム遅れ）。
  static bool IsGameHovered();
};
