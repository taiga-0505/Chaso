#pragma once
#include "Entity.h"
#include "Common/Math/MathUtils.h"
#include <string>

// Forward declarations
class Scene;
struct SceneContext;

/// @brief Base class for native scripts attached to entities.
/// Allows writing custom update logic similar to Unity's MonoBehaviour.
class ScriptableEntity {
public:
  virtual ~ScriptableEntity() = default;

  /// @brief Serialize script data to JSON
  virtual nlohmann::json Serialize() { return nlohmann::json::object(); }

  /// @brief Deserialize script data from JSON
  virtual void Deserialize(const nlohmann::json& j) {}

  /// @brief Called for drawing custom ImGui UI in the editor
  virtual void OnImGui() {}

  /// @brief Get a component attached to the same entity
  template <typename T> T* GetComponent() {
    return entity_->GetComponent<T>();
  }

  /// @brief Check if a component is attached to the same entity
  template <typename T> bool HasComponent() {
    return entity_->HasComponent<T>();
  }

  /// @brief Add a component to the same entity
  template <typename T, typename... Args> T& AddComponent(Args&&... args) {
    return entity_->AddComponent<T>(std::forward<Args>(args)...);
  }

  /// @brief Get the entity this script is attached to
  Entity* GetEntity() const { return entity_; }

  /// @brief Get the scene this script belongs to
  Scene* GetScene() const { return scene_; }

  /// @brief Get the current scene context
  SceneContext* GetSceneContext() const { return sceneContext_; }

  /// @brief Called during the render phase (command list is open)
  /// @details Use this for 2D drawing (DrawBox, DrawCircle, etc.)
  virtual void OnRender() {}

  /// @brief シャドウパス中に呼ばれる。「影を落とす（光を遮る）」3D ジオメトリだけをここで描く
  /// @details スクリプトが OnRender で自前に 3D モデルを描いている場合（マップの壁・床、ドアなど）、
  ///          ここでも同じモデルを RC::DrawModel / RC::DrawModelBatchColored で描くこと。
  ///          描いた物はシャドウマップ（平行光源）とスポットライト影アトラスの両方に深度として書かれ、
  ///          その向こう側へ光が漏れなくなる。
  ///          1 フレームに「影を落とすライトの数」だけ呼ばれるので、2D 描画（HUD 等）や
  ///          ギズモ、状態の更新は行わないこと。既定では何も描かない（＝影を落とさない）。
  virtual void OnShadowRender() {}

  /// @brief マスクパス中に呼ばれる。「輪郭を強調したい」3D ジオメトリだけをここで描く
  /// @details 描いた物のシルエットがマスクRTへ白く書かれ、`PostEffectType::MaskOutline`
  ///          がその外周だけを塗る（インタラクトできる扉・アイテムの強調など）。
  ///          描画コードは OnShadowRender と同じで、`RC::DrawModel` 系をそのまま呼べば
  ///          エンジンが PSO をマスク用へ振り替える。
  ///          強調していないフレームは「何も描かない」こと（＝そのまま return する）。
  ///          1 フレームに 1 回だけ呼ばれる。2D 描画や状態の更新は行わないこと。
  virtual void OnMaskRender() {}

  /// @brief Called during the overlay 3D phase to draw gizmos
  /// @details 呼ばれる条件: F4（全デバッグ描画）ON、またはこのエンティティが選択中
  virtual void OnDebugRender() {}

  /// @brief コライダーデバッグ表示（F3）中に呼ばれる
  /// @details 呼ばれる条件は ColliderComponent のギズモと同じ
  ///          （F3 ON / F4 ON / このエンティティが選択中）。
  ///          感知範囲・視界・攻撃範囲など、ColliderComponent を持たない
  ///          「見えない当たり判定」をワイヤで可視化する用途に使う。
  ///          再生中・停止中を問わず呼ばれるので、実際の判定に使っている値をそのまま描くこと。
  virtual void OnColliderDebugRender() {}

  /// @brief Called when a collision occurs with another entity
  /// @param other The entity that this entity collided with
  /// @param contactPoint The world position where the collision occurred
  virtual void OnCollision(Entity* other, const RC::Vector3& contactPoint = {}) {}

  /// @brief 当たり判定付きで移動する（壁抜け防止＋壁ずり）
  /// @param delta このフレームの移動量（速度 × deltaTime）
  /// @param maxStep 1回の判定で進む最大距離(m)。シーン内の最も薄い壁の厚みより
  ///                小さくしておくこと（既定 0.1m）
  /// @return 実際に移動した量
  /// @details TransformComponent::position を直接書き換える代わりにこれを使うと、
  ///          移動量が maxStep 以下に分割されるためダッシュ中でも壁を貫通しない。
  ///          斜め移動で壁に当たった場合は壁に沿ってスライドする。
  /// @note 実体は Application 側の Scene.cpp で定義している（Scene の完全型が必要なため）
  RC::Vector3 MoveAndSlide(const RC::Vector3& delta, float maxStep = 0.1f);

  /// @brief 指定したシーンへの遷移を要求する
  /// @param name 遷移先のシーン名（Resources/Scenes/*.json のファイル名）
  /// @return 要求が受理されたら true（未登録のシーン名、遷移中、編集モード中は false）
  /// @details 実際の切り替えはフェード演出を挟んで数フレーム後に行われるため、
  ///          呼び出した直後もこのスクリプトと所属エンティティは生存している。
  ///          即時切り替え（ChangeImmediately）は呼び出し元自身が解放されて
  ///          use-after-free になるため、スクリプトには公開していない。
  /// @note 再生中（PlayState::Playing）以外では何もしない。編集モードで遷移すると
  ///       保存していないシーン編集が失われるため。
  /// @note 実体は Application 側の Scene.cpp で定義している（SceneContext の完全型が必要なため）
  bool RequestSceneChange(const std::string& name);

protected:
  /// @brief Called when the script is created
  virtual void OnCreate() {}

  /// @brief Called every frame
  /// @param deltaTime Time elapsed since the last frame
  virtual void OnUpdate(float deltaTime) {}

  /// @brief Called when the script is destroyed
  virtual void OnDestroy() {}

private:
  Entity* entity_ = nullptr;
  Scene* scene_ = nullptr;
  SceneContext* sceneContext_ = nullptr;
  friend class NativeScriptComponent;
};
