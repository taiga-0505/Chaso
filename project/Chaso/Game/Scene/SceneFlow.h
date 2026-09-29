#pragma once
#include "Common/SceneContext.h"

#include <functional>
#include <string>
#include <vector>

// ============================================================================
// SceneFlow — シーン遷移表
// ----------------------------------------------------------------------------
// 「どのシーンで・何が起きたら・どこへ・どの演出で」遷移するかを 1 か所に集めた表。
// 保存先は Resources/SceneFlow.json。エディタの Window > Scene Flow で編集できる。
//
// スクリプトやゲームルールはシーン名を直接書かず、「きっかけ（trigger）」の名前だけで遷移する。
//
//   RequestTransition("start");          // Title の「スタート」
//   RequestTransition("stage", "Stage3"); // 行き先をスクリプトが決める（遷移先が $arg の行）
//
// 行の書き方
//   from       : 遷移元。シーン名 / "Stage*"（前方一致）/ "*"（全シーン）
//                複数の行が当てはまるときは「完全一致 → 長い前方一致 → *」の順で優先する
//   trigger    : きっかけの名前（スクリプト・GameMode が渡す文字列）
//   to         : 遷移先。シーン名、または $ で始まる変数
//                  $current … いまのシーン（やり直し）
//                  $arg     … スクリプトが RequestTransition の第 2 引数で渡した名前
//                  その他   … Application が RegisterVariable で登録したもの（例: $nextStage）
//   transition : 演出名（"dissolve" / "dive"）
//   note       : メモ（エディタに表示するだけ）
// ============================================================================

/// @brief 遷移表の 1 行
struct SceneFlowRule {
  std::string from;                       ///< 遷移元（シーン名 / "Prefix*" / "*"）
  std::string trigger;                    ///< きっかけ
  std::string to;                         ///< 遷移先（シーン名 / $変数）
  std::string transition = "dissolve";    ///< 演出名
  std::string note;                       ///< メモ
};

/// @brief 遷移表を引いた結果
struct SceneFlowResult {
  std::string target;                ///< 変数を解決した遷移先シーン名（空なら「行き先なし」）
  std::string transition;            ///< 演出名
  const SceneFlowRule *rule = nullptr; ///< 当てはまった行
};

/// @class SceneFlow
/// @brief シーン遷移表（シングルトン）
class SceneFlow {
public:
  /// @brief 既定の保存先（作業ディレクトリ = project/ 基準）
  static constexpr const char *kDefaultPath = "Resources/SceneFlow.json";

  /// @brief 組み込み変数
  static constexpr const char *kVarCurrent = "$current";
  static constexpr const char *kVarArg = "$arg";

  /// @brief 唯一のインスタンス
  static SceneFlow &Get();

  // ------------------------------------------------------------------
  // 読み書き
  // ------------------------------------------------------------------

  /// @brief JSON から読み込む（ファイルが無ければ空の表で始める）
  bool Load(const std::string &path = kDefaultPath);
  /// @brief 読み込んだ場所へ保存する
  bool Save();
  /// @brief 保存先パス
  const std::string &Path() const { return path_; }
  /// @brief 保存していない変更があるか
  bool IsDirty() const { return dirty_; }
  /// @brief 変更ありにする（エディタが行を書き換えたときに呼ぶ）
  void MarkDirty() { dirty_ = true; }

  /// @brief 行の一覧（エディタから直接編集してよい。編集したら MarkDirty）
  std::vector<SceneFlowRule> &Rules() { return rules_; }
  const std::vector<SceneFlowRule> &Rules() const { return rules_; }

  // ------------------------------------------------------------------
  // 解決
  // ------------------------------------------------------------------

  /// @brief from のシーンで trigger に当てはまる行を探す（無ければ nullptr）
  const SceneFlowRule *FindRule(const std::string &fromScene, const std::string &trigger) const;

  /// @brief 遷移先の変数を解決する（変数でなければそのまま返す）
  /// @param to 行の遷移先
  /// @param fromScene いまのシーン名（$current 用）
  /// @param arg スクリプトが渡した名前（$arg 用）
  /// @return 解決したシーン名。解決できなければ空
  std::string ResolveTarget(const std::string &to, const std::string &fromScene,
                            const std::string &arg) const;

  /// @brief from のシーンで trigger を引き、遷移先と演出を返す
  /// @return 行が見つかれば true（遷移先が空のこともある）。見つからなければ未登録として記録する
  bool Resolve(const std::string &fromScene, const std::string &trigger, const std::string &arg,
               SceneFlowResult &out);

  /// @brief エディタ表示用に「trigger → 行き先 (演出)」の文字列を作る
  /// @details 未登録なら「(未登録)」。Resolve と違って未登録の記録はしない（毎フレーム呼んでよい）
  std::string Describe(const std::string &fromScene, const std::string &trigger) const;

  /// @brief from の書き方が scene に当てはまるか
  /// @param score 当てはまったときの優先度（大きいほど優先）。不要なら nullptr
  static bool MatchFrom(const std::string &pattern, const std::string &scene, int *score = nullptr);

  /// @brief 使える演出名の一覧
  static const std::vector<std::string> &TransitionNames();

  // ------------------------------------------------------------------
  // 変数（Application 側が登録する）
  // ------------------------------------------------------------------

  /// @brief 遷移先の変数を登録する
  /// @param name "$" で始まる名前（例: "$nextStage"）
  /// @param description エディタに出す説明
  /// @param fn 解決関数。空文字を返すと「行き先なし」
  /// @details 静的初期化から呼んでよい。Load しても消えない。
  void RegisterVariable(const std::string &name, const std::string &description,
                        std::function<std::string()> fn);

  /// @brief 変数の情報（エディタ表示用）
  struct VariableInfo {
    std::string name;
    std::string description;
    bool builtin = false;
  };
  /// @brief 使える変数の一覧（組み込み → 登録順）
  std::vector<VariableInfo> Variables() const;
  /// @brief その名前の変数があるか
  bool HasVariable(const std::string &name) const;

  // ------------------------------------------------------------------
  // 未登録のきっかけ（エディタに「追加」ボタン付きで出す）
  // ------------------------------------------------------------------

  struct MissingTrigger {
    std::string from;
    std::string trigger;
    int count = 0;
  };
  const std::vector<MissingTrigger> &Missing() const { return missing_; }
  void RemoveMissing(const std::string &from, const std::string &trigger);

  // ------------------------------------------------------------------
  // エディタ向けの結線（SceneManager::Init が差す）
  // ------------------------------------------------------------------

  /// @brief 登録済みシーン名の取得関数を差す
  void SetSceneNameProvider(std::function<std::vector<std::string>()> fn) { sceneNames_ = std::move(fn); }
  /// @brief 登録済みシーン名（ソート済み）
  std::vector<std::string> SceneNames() const;
  /// @brief シーンが登録済みか
  bool IsKnownScene(const std::string &name) const;

  /// @brief いまのシーン名の取得関数を差す
  void SetCurrentSceneProvider(std::function<std::string()> fn) { currentScene_ = std::move(fn); }
  /// @brief いまのシーン名
  std::string CurrentScene() const { return currentScene_ ? currentScene_() : std::string(); }

  /// @brief 遷移要求の関数を差す（エディタの「試す」ボタンが使う）
  void SetRequestFn(std::function<bool(const std::string &, SceneTransition)> fn) { request_ = std::move(fn); }
  /// @brief 遷移を要求する（エディタ用。スクリプトは ScriptableEntity::RequestTransition を使う）
  bool Request(const std::string &target, const std::string &transition);

private:
  SceneFlow() = default;

  struct Variable {
    std::string name;
    std::string description;
    std::function<std::string()> fn;
  };

  std::string path_ = kDefaultPath;
  bool dirty_ = false;
  std::vector<SceneFlowRule> rules_;
  std::vector<Variable> variables_;
  std::vector<MissingTrigger> missing_;

  std::function<std::vector<std::string>()> sceneNames_;
  std::function<std::string()> currentScene_;
  std::function<bool(const std::string &, SceneTransition)> request_;
};
