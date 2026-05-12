#pragma once
#include "AppConfig.h"
#include "Dx12Core.h"
#include <Windows.h>
#include <d3d12.h>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"
#else
struct ImFont; // Releaseでは型だけ前方宣言
#endif

/// @class ImGuiManager
/// @brief ImGui の初期化、更新、描画を管理するクラス
/// @details DirectX12 バックエンドを使用した ImGui のライフサイクルを管理します。
/// RC_ENABLE_IMGUI マクロによって有効/無効が切り替わり、日本語フォントのロード機能も備えています。
class ImGuiManager {
public:
  /// @brief 初期化
  /// @param hwnd ウィンドウハンドル
  /// @param core Dx12Core インスタンス
  /// @param enableDocking ドッキング機能を有効にするか
  /// @param jpFontSize 日本語フォントのサイズ
  /// @param jpFontPath 日本語フォント（TTF）のパス
  void
  Init(HWND hwnd, Dx12Core &core, bool enableDocking = true, float jpFontSize = 15.0f,
       const char *jpFontPath = "Resources/fonts/Huninn/Huninn-Regular.ttf");

  /// @brief 新規フレームの開始を宣言する
  void NewFrame();

  /// @brief 描画コマンドをコマンドリストに記録する
  /// @param cmdList グラフィックスコマンドリスト
  void Render(ID3D12GraphicsCommandList *cmdList);

  /// @brief 終了処理
  void Shutdown();

  /// @brief ImGui が有効かつ初期化済みか
  /// @return 有効なら true
  bool Enabled() const { return RC_ENABLE_IMGUI && initialized_; }

  /// @brief デフォルトフォントを取得する
  /// @return ImFont ポインタ
  ImFont *GetDefaultFont() const { return fontDefault_; }

  /// @brief 日本語フォントを取得する
  /// @return ImFont ポインタ
  ImFont *GetJPFont() const { return fontJP_; }

private:
  bool initialized_ = false;       ///< 初期化フラグ
  ImFont *fontDefault_ = nullptr; ///< デフォルトフォント
  ImFont *fontJP_ = nullptr;      ///< 日本語フォント
  bool dockingEnabled_ = false;    ///< ドッキング機能が有効か

  /// @brief ImGui 用の SRV スロットを予約する
  /// @param core Dx12Core インスタンス
  void reserveSrvSlotForImGui_(Dx12Core &core);
};
