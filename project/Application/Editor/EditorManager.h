#pragma once
#include <d3d12.h>
#include <string>
#include <functional>
#include <memory>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include "Particle/GPUParticle.h"
#include "Graphics/Texture/RenderTexture/RenderTexture.h"
#include "Camera/CameraController.h"

class Dx12Core;
class Scene;
class Entity;
enum class PlayState;

/// @class EditorManager
/// @brief ImGuiによるエディタUI（ドッキングレイアウト、テーマ、各種パネル）を管理するクラス
class EditorManager {
public:
  EditorManager() = default;
  ~EditorManager() = default;

  /// @brief エディタの初期化（テーマ設定など）
  void Initialize();

  /// @brief エディタの終了処理（リソースの解放）
  void Term();

  /// @brief 毎フレームの更新処理（メニューバーやドッキングスペースの構築）
  /// @param core Dx12Core インスタンスへのポインタ
  /// @param onMenuAppend メニューバー追加用コールバック
  /// @param currentScene 現在のシーン
  void Update(Dx12Core* core = nullptr, std::function<void()> onMenuAppend = nullptr, Scene* currentScene = nullptr);

  /// @brief 各種パネルの描画
  /// @param viewportSrv ゲーム画面（Viewport）に表示するテクスチャのSRVハンドル
  /// @param core Dx12Core インスタンスへのポインタ
  /// @param pm PipelineManagerへのポインタ
  /// @param deltaTime 前のフレームからの経過時間
  /// @param currentScene 現在のシーン
  void DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core = nullptr, class PipelineManager* pm = nullptr, float deltaTime = 0.0f, Scene* currentScene = nullptr);

  /// @brief Viewportウィンドウがホバーされているか取得する
  bool IsViewportHovered() const { return isViewportHovered_; }
  void SetViewportHovered(bool hovered) { isViewportHovered_ = hovered; }

  /// @brief Particle Editor用の専用プレビュー画面を描画する
  /// @param cl 描画用コマンドリスト
  /// @param core Dx12Coreのインスタンス
  /// @param pm PipelineManagerへのポインタ
  /// @param deltaTime デルタタイム
  void RenderParticlePreview(ID3D12GraphicsCommandList* cl, Dx12Core* core, class PipelineManager* pm, float deltaTime);

  /// @brief 現在のRender Queueの状態をテキストダンプとして出力する
  void ExportRenderQueueDump();

  void SaveConfig();
  void LoadConfig();

  /// @brief 現在の再生状態を取得する
  PlayState GetPlayState() const;

  /// @brief 再生状態を設定する（外部からのリセット用）
  void SetPlayState(PlayState state);

  /// @brief 再起動（リスタート）がリクエストされたか
  bool IsRestartRequested() const { return restartRequested_; }
  void ClearRestartRequest() { restartRequested_ = false; }

  /// @brief 選択中のエンティティIDを取得する（未選択なら0）
  uint32_t GetSelectedEntityId() const;

  struct ResizeRequest {
    bool pending = false;
    int width = 0;
    int height = 0;
    bool fullscreen = false;
  };

  /// @brief ウィンドウリサイズの要求を取得
  const ResizeRequest& GetResizeRequest() const { return resizeRequest_; }
  void ClearResizeRequest() { resizeRequest_.pending = false; }

private:
  /// @brief ダークテーマを適用する
  void ApplyDarkTheme();

  /// @brief 初回起動時やリセット時にデフォルトのドッキングレイアウトを構築する
  void SetupDockingLayout();

  /// @brief ツリーノードとしてエンティティを描画する（再帰）
  void DrawEntityNode(std::shared_ptr<Entity> e, Scene* currentScene, const std::unordered_map<uint64_t, std::vector<std::shared_ptr<Entity>>>& childrenMap);

  /// @brief 空のオブジェクト（Transform のみ）を生成して選択状態にする
  /// @param currentScene 現在のシーン
  /// @param parentGuid 親エンティティの GUID（0 でルート）
  /// @return 生成したエンティティ（シーンが無い場合は nullptr）
  std::shared_ptr<Entity> CreateEmptyEntity(Scene* currentScene, uint64_t parentGuid = 0);

  // ============================================================
  // ショートカット / Undo・Redo / コピー＆ペースト
  // ============================================================

  /// @brief F2・Ctrl+C/V/Z/Y などのエディタショートカットを処理する
  /// @param core Dx12Core（スクリーンショット要求用）
  /// @param currentScene 現在のシーン
  /// @details ImGui フレーム内（EditorManager::Update の先頭）で呼ぶこと
  void HandleShortcuts(Dx12Core* core, Scene* currentScene);

  /// @brief 前回コミット時からシーンが変化していれば Undo 履歴へ積む
  /// @param currentScene 現在のシーン
  /// @details ウィジェット操作中・ギズモ操作中は積まないため、
  ///          1回のドラッグ操作が1ステップにまとまる。
  void CommitHistoryIfChanged(Scene* currentScene);

  /// @brief Undo 履歴を初期化する（シーン切り替え時など）
  void ResetHistory(Scene* currentScene);

  /// @brief 1ステップ戻す
  void Undo(Scene* currentScene);

  /// @brief 1ステップ進める
  void Redo(Scene* currentScene);

  /// @brief 選択中のエンティティ（と子孫）を内部クリップボードへコピーする
  void CopySelectedEntity(Scene* currentScene);

  /// @brief 内部クリップボードの内容を新しいエンティティとして貼り付ける
  void PasteEntityClipboard(Scene* currentScene);

  /// @brief スナップショットを適用し、GUID を頼りに選択状態を復元する
  void ApplySnapshot(Scene* currentScene, const nlohmann::json& snapshot);

private:
  bool firstLayout_ = true; ///< 初回レイアウト構築フラグ
  bool resetLayout_ = false; ///< レイアウトリセット要求フラグ
  bool showDemoWindow_ = false; ///< ImGuiデモウィンドウの表示フラグ
  bool showPerfWindow_ = false; ///< パフォーマンス（FPS）ウィンドウの表示フラグ
  bool showRenderQueue_ = false; ///< Render Queue デバッグウィンドウの表示フラグ
  bool showParticleEditor_ = false; ///< Particle Editor ウィンドウの表示フラグ
  bool showEnvironmentWindow_ = false; ///< Environment Settings ウィンドウの表示フラグ
  bool showPostEffectWindow_ = false; ///< Post Effect Settings ウィンドウの表示フラグ
  bool isViewportHovered_ = false; ///< Viewportウィンドウがホバーされているか

  PlayState playState_; ///< エディタ上での現在の再生状態
  bool restartRequested_ = false; ///< リスタート要求フラグ

  int playIconTex_ = -1;
  int pauseIconTex_ = -1;
  int stopIconTex_ = -1;
  int restartIconTex_ = -1;

  int eyeVisibleTex_ = -1;
  int eyeHiddenTex_ = -1;
  int lockLockedTex_ = -1;
  int lockUnlockedTex_ = -1;

  int folderIconTex_ = -1;
  int fileIconTex_ = -1;
  int fileImageTex_ = -1;
  int file3DTex_ = -1;
  int fileMaterialTex_ = -1;
  int fileDocTex_ = -1;
  int fileFontTex_ = -1;

  std::weak_ptr<Entity> selectedEntity_; ///< Inspector表示用の選択エンティティ
  bool dragging2D_ = false;      ///< Viewport 上で 2D 要素（Text/Sprite）をドラッグ中か
  int drag2DMode_ = 0;           ///< 0=なし 1=移動 2=四隅ハンドルでサイズ変更
  float drag2DAnchorX_ = 0.0f;   ///< サイズ変更時に固定する対角コーナー（ゲーム px）
  float drag2DAnchorY_ = 0.0f;
  float drag2DBaseW_ = 1.0f;     ///< サイズ変更開始時の矩形幅（ゲーム px）
  float drag2DBaseH_ = 1.0f;     ///< サイズ変更開始時の矩形高さ（ゲーム px）
  float drag2DBaseScale_ = 1.0f; ///< サイズ変更開始時の Text scale
  float drag2DLastX_ = 0.0f;     ///< 2D ドラッグの前フレームマウス座標 X
  float drag2DLastY_ = 0.0f;     ///< 2D ドラッグの前フレームマウス座標 Y
  uint32_t renamingEntityId_ = 0; ///< 名前変更中のエンティティID
  bool focusRename_ = false; ///< 名前変更用のフォーカスフラグ
  uint64_t expandEntityGuid_ = 0; ///< 子を追加した直後に Hierarchy で展開する親の GUID（0 で無効）
  std::filesystem::path currentDirectory_ = "Resources"; ///< コンテンツブラウザの現在ディレクトリ

  ResizeRequest resizeRequest_; ///< ウィンドウリサイズ要求

  // --- Undo / Redo（スナップショット方式） ---
  bool historyEnabled_ = true;                ///< 履歴機能のオン/オフ（切り分け用）
  static constexpr size_t kMaxHistory = 50;   ///< 履歴の最大数
  static constexpr float kHistoryPollInterval = 0.15f; ///< 変更検知の間隔（秒）
  std::vector<nlohmann::json> undoStack_;     ///< 変更前の状態（古い順）
  std::vector<nlohmann::json> redoStack_;     ///< やり直し用の状態
  nlohmann::json historyCurrent_;             ///< 直近にコミットした状態
  size_t historyHash_ = 0;                    ///< historyCurrent_ のハッシュ
  nlohmann::json historyPending_;             ///< 確定待ちの状態（変化が落ち着くまで保留）
  size_t historyPendingHash_ = 0;             ///< historyPending_ のハッシュ（0 なら保留なし）
  bool historyValid_ = false;                 ///< 履歴が初期化済みか
  bool historyResync_ = false;                ///< 復元直後（次の差分は履歴に積まない）
  bool historyErrorLogged_ = false;           ///< スナップショット失敗を一度だけログする
  int historyBlockLogged_ = -1;               ///< 履歴が止まっている理由（重複ログ抑制用）
  const void* historySceneKey_ = nullptr;     ///< 履歴の対象シーン（切り替え検知用）
  float historyPollTimer_ = 0.0f;             ///< 変更検知のタイマー

  // --- エンティティのコピー＆ペースト ---
  nlohmann::json entityClipboard_; ///< コピーしたエンティティ群（先頭がルート）

  // --- Gizmo Settings ---
  int gizmoOperation_ = 7; // ImGuizmo::TRANSLATE
  int gizmoMode_ = 0;      // ImGuizmo::LOCAL

  // --- Particle Editor ---
  std::unique_ptr<GPUParticle> peParticle_; ///< Particle Editor 用のプレビュー用 GPUParticle
  bool peInitialized_ = false;  ///< Particle Editor が初期化済みか
  char peJsonPath_[256] = "Resources/Particle/default.json"; ///< JSON 保存/読み込みパス
  char peTexPath_[256] = "Resources/Particle/circle.png"; ///< テクスチャパス

  // プレビュー描画用リソース
  RenderTexture peRenderTexture_; ///< プレビュー描画先
  RC::CameraController peCamera_; ///< プレビュー専用カメラ
};
