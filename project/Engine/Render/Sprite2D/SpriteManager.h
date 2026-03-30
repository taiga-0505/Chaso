#pragma once

// SpriteManager
// -----------------------------------------------------------------------------
// 「スプライト周り」の管理クラス
// - スプライトはハンドル制（int）
// - Sprite2D 実体の生成/破棄/状態変更をここに集約
// - 全スプライト共通の Quad( SpriteMesh2D ) もここで共有
// -----------------------------------------------------------------------------

#include <d3d12.h>

#include <memory>
#include <string>
#include <vector>

#include "Sprite2D/Sprite2D.h"

class TextureManager; // 前方宣言

namespace RC {

class SpriteManager {
public:
  /// <summary>
  /// 初期化（device/texman を注入）
  /// </summary>
  void Init(ID3D12Device *device, TextureManager *texman);

  /// <summary>
  /// 終了（管理しているスプライトと共有 Quad を解放）
  /// </summary>
  void Term();

  /// <summary>
  /// スプライトを生成してハンドルを返す
  /// </summary>
  /// <param name="path">画像パス</param>
  /// <param name="screenW">画面幅（ピクセル）</param>
  /// <param name="screenH">画面高さ（ピクセル）</param>
  /// <param name="srgb">sRGB として読むか</param>
  /// <returns>スプライトハンドル（失敗時 -1）</returns>
  int Load(const std::string &path, float screenW, float screenH,
           bool srgb = true);

  /// <summary>
  /// スプライトを解放（ハンドルは無効化）
  /// </summary>
  void Unload(int handle);

  /// <summary>
  /// 有効ハンドルか？
  /// </summary>
  bool IsValid(int handle) const;

  /// <summary>
  /// 実体を取得（無効なら nullptr）
  /// </summary>
  Sprite2D *Get(int handle);
  const Sprite2D *Get(int handle) const;

  /// <summary>
  /// 通常描画（Pipeline のバインドは呼び出し側が行う想定）
  /// </summary>
  void Draw(int handle, ID3D12GraphicsCommandList *cl);

  /// <summary>
  /// テクスチャ内矩形（ピクセル）指定で描画
  /// </summary>
  void DrawRect(int handle, float srcX, float srcY, float srcW, float srcH,
                float texW, float texH, float insetPx,
                ID3D12GraphicsCommandList *cl);

  /// <summary>
  /// UV(0..1) 矩形指定で描画
  /// </summary>
  void DrawRectUV(int handle, float u0, float v0, float u1, float v1,
                  ID3D12GraphicsCommandList *cl);

  /// <summary>
  /// Transform を設定
  /// </summary>
  void SetTransform(int handle, const Transform &t);

  /// <summary>
  /// 乗算カラーを設定
  /// </summary>
  void SetColor(int handle, const Vector4 &color);

  /// <summary>
  /// 表示サイズ（ピクセル）を設定
  /// </summary>
  void SetSize(int handle, float w, float h);

  /// <summary>
  /// ImGui 表示
  /// </summary>
  void DrawImGui(int handle, const char *name);

  /// <summary>
  /// 便利用：生成元のテクスチャハンドルを返す（無効なら -1）
  /// </summary>
  int GetTexHandle(int handle) const;

private:
  struct Slot {
    std::unique_ptr<Sprite2D> ptr;
    bool inUse = false;
    int texHandle = -1;
  };

  std::shared_ptr<SpriteMesh2D> EnsureQuad_();

private:
  ID3D12Device *device_ = nullptr;
  TextureManager *texman_ = nullptr;

  std::vector<Slot> sprites_;
  std::shared_ptr<SpriteMesh2D> quad_; // 全スプライト共通
};

} // namespace RC
