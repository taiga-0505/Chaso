#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include "struct.h" // TextAlign
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

/// @brief 画面上に文字列を描画するコンポーネント
/// @details
/// - 位置は TransformComponent の position.x / position.y（ピクセル、左上原点）を使用します。
/// - フォントは RC::LoadFont で読み込まれ、fontHandle に保持されます（シーンロード時に自動）。
/// - 描画は DataDrivenScene の 2D パスで RC::DrawString により行われます。
class TextRendererComponent : public IComponent {
public:
  // ---- シリアライズ対象 ----
  std::string text = "Text";                       ///< 表示文字列（UTF-8、'\n' で改行）
  std::string fontPath =
      "Resources/fonts/Kiwi_Maru/KiwiMaru-Regular.ttf"; ///< フォントファイル
  float fontSize = 32.0f;                          ///< フォントサイズ（ピクセル）
  uint32_t atlasSize = 1024;                       ///< グリフアトラスの一辺
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};    ///< 文字色
  float scale = 1.0f;                              ///< 拡大率
  TextAlign align = TextAlign::Left;               ///< 水平揃え
  float lineSpacing = 1.0f;                        ///< 行送り倍率
  bool visible = true;                             ///< 表示フラグ

  // ---- ランタイム ----
  int fontHandle = -1;      ///< RC::LoadFont のハンドル（保存しない）
  std::string loadedPath;   ///< fontHandle が指すフォントパス（再ロード判定用）
  float loadedSize = 0.0f;  ///< fontHandle が指すサイズ（再ロード判定用）

  bool HasFont() const { return fontHandle >= 0; }

  /// @brief 設定（fontPath / fontSize）とロード済みフォントが食い違っているか
  bool NeedsReload() const {
    return fontHandle < 0 || loadedPath != fontPath || loadedSize != fontSize;
  }

  const char *TypeName() const override { return "TextRendererComponent"; }

  nlohmann::json Serialize() const override {
    return {
        {"text", text},
        {"fontPath", fontPath},
        {"fontSize", fontSize},
        {"atlasSize", atlasSize},
        {"color", {color.x, color.y, color.z, color.w}},
        {"scale", scale},
        {"align", static_cast<int>(align)},
        {"lineSpacing", lineSpacing},
        {"visible", visible},
    };
  }

  void Deserialize(const nlohmann::json &j) override {
    if (j.contains("text")) text = j["text"].get<std::string>();
    if (j.contains("fontPath")) fontPath = j["fontPath"].get<std::string>();
    if (j.contains("fontSize")) fontSize = j["fontSize"].get<float>();
    if (j.contains("atlasSize")) atlasSize = j["atlasSize"].get<uint32_t>();
    if (j.contains("color")) {
      auto &c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
               c[3].get<float>()};
    }
    if (j.contains("scale")) scale = j["scale"].get<float>();
    if (j.contains("align")) {
      const int a = j["align"].get<int>();
      align = (a >= 0 && a <= 2) ? static_cast<TextAlign>(a) : TextAlign::Left;
    }
    if (j.contains("lineSpacing")) lineSpacing = j["lineSpacing"].get<float>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
  }
};
