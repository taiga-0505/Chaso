#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/// @brief Bone (socket) attachment component.
/// Makes this entity's model follow a joint of another entity's skeleton.
/// Typical use: a weapon entity attached to the "R_Hand" joint of a character.
///
/// @details 追従先は GUID で指定する。実際の追従は Transform 同期の後に行われ、
///          RC::SetModelWorldOverride() で描画行列を直接上書きするため、
///          このエンティティの TransformComponent の TRS は描画に使われなくなる。
///          位置を微調整したい場合は offsetPosition / offsetRotation / offsetScale を使う。
class BoneAttachmentComponent : public IComponent {
public:
  uint64_t targetGuid = 0;   ///< 追従先エンティティの GUID（0 = 未設定）
  std::string jointName;     ///< 追従する Joint 名（空 = 追従先のモデル原点）

  RC::Vector3 offsetPosition = {0.0f, 0.0f, 0.0f}; ///< Joint からの位置オフセット
  RC::Vector3 offsetRotation = {0.0f, 0.0f, 0.0f}; ///< 回転オフセット（オイラー角・ラジアン）
  RC::Vector3 offsetScale    = {1.0f, 1.0f, 1.0f}; ///< スケールオフセット

  // --- Runtime state (not serialized) ---
  bool overrideActive_ = false;              ///< ワールド行列を上書き中か（解除漏れ防止用）
  std::vector<std::string> jointNamesCache_; ///< Inspector のプルダウン用キャッシュ
  bool jointNamesDirty_ = true;              ///< キャッシュの再取得が必要か

  /// @brief 追従先が変わった時に呼ぶ。Joint 名の一覧を取り直す
  void MarkTargetDirty() { jointNamesDirty_ = true; }

  const char* TypeName() const override { return "BoneAttachmentComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"targetGuid", targetGuid},
      {"jointName", jointName},
      {"offsetPosition", {offsetPosition.x, offsetPosition.y, offsetPosition.z}},
      {"offsetRotation", {offsetRotation.x, offsetRotation.y, offsetRotation.z}},
      {"offsetScale",    {offsetScale.x,    offsetScale.y,    offsetScale.z}}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("targetGuid")) targetGuid = j["targetGuid"].get<uint64_t>();
    if (j.contains("jointName")) jointName = j["jointName"].get<std::string>();
    if (j.contains("offsetPosition")) {
      auto& p = j["offsetPosition"];
      offsetPosition = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
    }
    if (j.contains("offsetRotation")) {
      auto& r = j["offsetRotation"];
      offsetRotation = {r[0].get<float>(), r[1].get<float>(), r[2].get<float>()};
    }
    if (j.contains("offsetScale")) {
      auto& s = j["offsetScale"];
      offsetScale = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
    }
    MarkTargetDirty();
  }
};
