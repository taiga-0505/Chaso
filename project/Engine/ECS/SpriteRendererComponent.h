#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief スプライトをどの空間で描画するか
enum class SpriteSpace : int {
  Screen = 0,   ///< スクリーン座標。3Dより手前（従来の挙動）
  ScreenBehind, ///< スクリーン座標。常に3Dより奥（背景用）
  World,        ///< ワールド座標。深度テストありでモデルと前後関係がつく
};

/// @brief Component for 2D sprite rendering.
/// Holds a sprite handle and screen size, automatically drawn in the entity loop.
class SpriteRendererComponent : public IComponent {
public:
  int spriteHandle = -1;  ///< Sprite handle (from RC::LoadSprite)
  bool visible = true;    ///< Visibility flag
  RC::Vector2 size = {100.0f, 100.0f}; ///< Display size (screen coords, pixels)
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< Multiply color

  /// @brief 描画する空間
  /// @details Screen       … 従来通り。x/y はピクセル、常にモデルより手前。
  ///          ScreenBehind … x/y はピクセルのまま、常にモデルより奥（背景）。
  ///          World        … Transform をワールド座標として扱い、深度テストあり。
  ///                          任意のモデルとモデルの間に挟み込めます。
  ///                          大きさは Size ではなく Transform.scale で決まります。
  SpriteSpace space = SpriteSpace::Screen;

  /// @brief ワールド空間モードか
  bool IsWorldSpace() const { return space == SpriteSpace::World; }

  /// @brief Check if a valid sprite is assigned
  bool HasSprite() const { return spriteHandle >= 0; }

  std::string spritePath; ///< Asset path for serialization
  std::string loadedPath; ///< Path currently loaded into spriteHandle (runtime only, for lazy reload)

  const char* TypeName() const override { return "SpriteRendererComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"spritePath", spritePath},
      {"visible", visible},
      {"size", {size.x, size.y}},
      {"color", {color.x, color.y, color.z, color.w}},
      {"space", static_cast<int>(space)}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("spritePath")) spritePath = j["spritePath"].get<std::string>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("size")) {
      auto& s = j["size"];
      size = {s[0].get<float>(), s[1].get<float>()};
    }
    if (j.contains("color")) {
      auto& c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
    if (j.contains("space")) {
      space = static_cast<SpriteSpace>(j["space"].get<int>());
    } else if (j.contains("behindModel")) {
      // 旧フォーマット互換
      space = j["behindModel"].get<bool>() ? SpriteSpace::ScreenBehind
                                           : SpriteSpace::Screen;
    }
  }
};
