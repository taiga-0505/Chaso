#pragma once
#include <cstdint>
#include <d3d12.h>
#include <d3d12.h>
#include <memory>
#include <string>
#include <iostream>
#include <wrl/client.h>

#include "Math/Math.h"
#include "function/function.h"
#include "imgui/imgui.h"

#include "SpriteMesh2D.h" 

/// @class Sprite2D
/// @brief 2D スプライト（テクスチャ付き矩形）の描画と管理を行うクラス
/// @details 共有メッシュ（SpriteMesh2D）を使用し、個別のトランスフォームやマテリアル（カラー、UV変換）を保持して描画します。
class Sprite2D {
public:
  Sprite2D() = default;
  ~Sprite2D();

  /// @brief 初期化
  /// @param device D3D12 デバイス
  /// @param mesh 共有頂点データ（SpriteMesh2D）の shared_ptr
  /// @param screenWidth 画面幅（行列計算用）
  /// @param screenHeight 画面高さ（行列計算用）
  void Initialize(ID3D12Device *device,
                  const std::shared_ptr<SpriteMesh2D> &mesh, float screenWidth,
                  float screenHeight);

  /// @brief 更新処理（トランスフォーム行列の再計算と GPU 転送）
  void Update();

  /// @brief 描画実行
  /// @param cmdList コマンドリスト
  void Draw(ID3D12GraphicsCommandList *cmdList) const;

  // ===== setters / getters =====

  /// @brief 使用するテクスチャ（SRV）を設定する
  /// @param srv テクスチャの GPU ハンドル
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { srv_ = srv; }

  /// @brief 画面サイズを更新する
  /// @param w 幅
  /// @param h 高さ
  void SetScreenSize(float w, float h);

  /// @brief スプライトのサイズ（ピクセル）を設定する
  /// @param w 幅
  /// @param h 高さ
  void SetSize(float w, float h);

  /// @brief 可視状態を設定する
  /// @param v true で表示
  void SetVisible(bool v) { visible_ = v; }

  /// @brief 可視状態か確認する
  /// @return 可視なら true
  bool Visible() const { return visible_; }

  /// @brief スプライトのカラー（乗算色）を設定する
  /// @param color RGBA カラー
  void SetColor(const RC::Vector4 &color) {
    cbMat_.map->color = RC::Vector4(color.x, color.y, color.z, color.w);
  }

  /// @brief スプライトのカラー（乗算色）を取得する
  /// @return RGBA カラー
  RC::Vector4 GetColor() { return cbMat_.map->color; }

  /// @brief トランスフォーム情報を取得する（読み書き可能）
  /// @return Transform への参照
  Transform &T() { return transform_; }

  /// @brief マテリアル情報を取得する（読み書き可能）
  /// @return SpriteMaterial 構造体へのポインタ
  SpriteMaterial *Mat() { return cbMat_.map; }

  /// @brief UV 変換行列を取得する（読み書き可能）
  /// @return UV 変換行列への参照
  RC::Matrix4x4 &UVTransform() { return cbMat_.map->uvTransform; }

  /// @brief 関連付けられたファイルパスを保存する（アセット管理用）
  /// @param path ファイルパス
  void SetFilePath(const std::string &path) { filePath_ = path; }

  /// @brief 関連付けられたファイルパスを取得する
  /// @return ファイルパス文字列
  const std::string &GetFilePath() const { return filePath_; }

  /// @brief ImGui を使用したデバッグ用 UI を表示する
  /// @param name 表示名
  void DrawImGui(const char *name);

private:
  /// @struct CBW
  /// @brief 行列用定数バッファ管理構造体
  struct CBW {
    Microsoft::WRL::ComPtr<ID3D12Resource> res;
    TransformationMatrix *map = nullptr;
  };

  /// @struct CBM
  /// @brief マテリアル用定数バッファ管理構造体
  struct CBM {
    Microsoft::WRL::ComPtr<ID3D12Resource> res;
    SpriteMaterial *map = nullptr;
  };

  /// @brief リソースの解放処理
  void Release();

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  std::shared_ptr<SpriteMesh2D> mesh_; ///< 共有メッシュ（頂点/インデックスバッファ）

  float screenW_ = 0.0f;
  float screenH_ = 0.0f;

  CBW cbWVP_{};
  CBM cbMat_{};

  D3D12_GPU_DESCRIPTOR_HANDLE srv_{};

  RC::Matrix4x4 view_{};
  RC::Matrix4x4 proj_{};

  Transform transform_{{100, 100, 1}, {0, 0, 0}, {0, 0, 0}}; ///< スプライトの座標・回転・サイズ

  bool visible_ = true;
  std::string filePath_;
};
