#pragma once

// ============================================================================
// ModelRendererComponent
// ----------------------------------------------------------------------------
// モデル描画コンポーネント。RC::LoadModel のハンドルを保持し、
// RenderSystem が自動で描画を行う。
// ============================================================================

#include "IComponent.h"
#include "Math/MathTypes.h"

class ModelRendererComponent : public IComponent {
public:
  /// RC::LoadModel() の戻り値
  int modelHandle = -1;

  /// テクスチャオーバーライド（-1 = マテリアルデフォルト）
  int texOverride = -1;

  /// 描画の可視性
  bool visible = true;

  /// ブレンドモード（将来拡張用）
  int blendMode = 0;

  /// 有効なモデルを保持しているか
  bool HasModel() const { return modelHandle >= 0; }
};
