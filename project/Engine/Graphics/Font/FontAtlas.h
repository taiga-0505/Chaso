#pragma once
#include <cstdint>
#include <d3d12.h>
#include <dwrite_2.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

#include "SRVManager/SRVManager.h"

/// @struct GlyphInfo
/// @brief アトラス上に登録された 1 文字分の情報
/// @details 座標はすべてピクセル単位。offsetX / offsetY はペン位置（ベースライン上）から
/// グリフビットマップ左上へのオフセットで、offsetY は通常負の値（上方向）になります。
struct GlyphInfo {
  float u0 = 0.0f, v0 = 0.0f; ///< アトラス UV 左上
  float u1 = 0.0f, v1 = 0.0f; ///< アトラス UV 右下
  float offsetX = 0.0f;       ///< ペン位置 → ビットマップ左上 X
  float offsetY = 0.0f;       ///< ベースライン → ビットマップ上端 Y（上方向が負）
  float width = 0.0f;         ///< ビットマップ幅
  float height = 0.0f;        ///< ビットマップ高さ
  float advance = 0.0f;       ///< 次の文字までのペン送り量
  bool hasBitmap = false;     ///< 描画する絵があるか（空白などは false）
};

/// @class FontAtlas
/// @brief DirectWrite でフォントファイル（TTF/OTF）からグリフをラスタライズし、
///        R8_UNORM のテクスチャアトラスへ動的に登録・GPU 転送するクラス
/// @details
/// - ラスタライズは DirectWrite（IDWriteGlyphRunAnalysis）のみで行い、D2D / D3D11On12 / ImGui には依存しません。
/// - グリフは GetGlyph() で初めて要求されたときにラスタライズされ、シェルフ方式でアトラスに詰められます。
/// - CPU 側アトラスの更新は Flush() で「汚れた行の帯」だけをアップロードします。
///   Flush は描画コマンドリストに Copy を記録するため、同一フレーム内の後続 Draw から参照できます。
/// - 1 インスタンス = 1 フォントファイル × 1 ピクセルサイズ。別サイズは別インスタンスを作ってください
///   （描画時の scale 引数で拡縮も可能ですが、大きく拡大するとぼやけます）。
class FontAtlas {
public:
  /// @brief アトラス幅・高さの最小値（RowPitch を 512 バイト境界に揃えるため）
  static constexpr uint32_t kMinAtlasSize = 512;
  /// @brief アップロード用ステージングのリング数（トリプルバッファ）
  static constexpr uint32_t kStagingCount = 3;
  /// @brief グリフ間のパディング（にじみ防止）
  static constexpr uint32_t kPadding = 1;

  FontAtlas() = default;
  ~FontAtlas();

  FontAtlas(const FontAtlas &) = delete;
  FontAtlas &operator=(const FontAtlas &) = delete;

  /// @brief 初期化
  /// @param srv SRV 管理クラス
  /// @param path フォントファイルのパス（UTF-8。.ttf / .otf / .ttc）
  /// @param sizePx フォントサイズ（ピクセル。em 高さ）
  /// @param atlasSize アトラスの一辺（512 の倍数に丸められる。例: 1024, 2048）
  /// @param faceIndex TTC 等でフェイスが複数ある場合のインデックス
  /// @return 成功なら true
  bool Initialize(SRVManager *srv, const std::string &path, float sizePx,
                  uint32_t atlasSize = 1024, uint32_t faceIndex = 0);

  /// @brief 解放（SRV も解放する）
  /// @param srv SRV 管理クラス（GPU が参照を終えていること）
  void Term(SRVManager *srv);

  /// @brief グリフ情報を取得する（未登録ならラスタライズしてアトラスへ追加）
  /// @param codepoint Unicode コードポイント（UTF-32）
  /// @return グリフ情報。フォントに無い/アトラス満杯などで失敗した場合は nullptr
  const GlyphInfo *GetGlyph(uint32_t codepoint);

  /// @brief 未転送のグリフを GPU テクスチャへ転送する
  /// @param cl 描画に使用しているコマンドリスト（Copy と Barrier を記録）
  /// @param frameIndex フレームインデックス（ステージングのリング選択に使用）
  /// @note 汚れがなければ何もしません。Draw の直前に毎回呼んで構いません。
  void Flush(ID3D12GraphicsCommandList *cl, uint32_t frameIndex);

  /// @brief 転送待ちのグリフがあるか
  bool IsDirty() const { return dirtyMinY_ <= dirtyMaxY_; }

  // ===== getters =====

  /// @brief アトラステクスチャの GPU SRV ハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE Srv() const { return srv_.gpu; }
  /// @brief 初期化済みか
  bool IsLoaded() const { return fontFace_ != nullptr && texture_ != nullptr; }
  /// @brief フォントサイズ（ピクセル）
  float SizePx() const { return sizePx_; }
  /// @brief ベースラインから上端までの高さ（ピクセル）
  float Ascent() const { return ascent_; }
  /// @brief ベースラインから下端までの高さ（ピクセル、正の値）
  float Descent() const { return descent_; }
  /// @brief 行送り量（ascent + descent + lineGap）
  float LineHeight() const { return lineHeight_; }
  /// @brief アトラスの一辺（ピクセル）
  uint32_t AtlasSize() const { return atlasSize_; }
  /// @brief フォントファイルパス
  const std::string &Path() const { return path_; }
  /// @brief 登録済みグリフ数
  size_t GlyphCount() const { return glyphs_.size(); }
  /// @brief アトラスが満杯になって登録に失敗したことがあるか
  bool IsAtlasFull() const { return atlasFull_; }

private:
  bool CreateFontFace_(const std::string &path, uint32_t faceIndex);
  bool CreateTexture_(SRVManager *srv);
  bool Rasterize_(uint32_t codepoint, GlyphInfo &out);
  bool AllocRect_(uint32_t w, uint32_t h, uint32_t &outX, uint32_t &outY);
  void MarkDirty_(uint32_t y0, uint32_t y1);

private:
  // --- DirectWrite ---
  Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2_; ///< グレースケール AA 用（無ければ nullptr）
  Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace_;
  DWRITE_FONT_METRICS fontMetrics_{};

  std::string path_;
  float sizePx_ = 0.0f;
  float designScale_ = 0.0f; ///< デザイン単位 → ピクセル
  float ascent_ = 0.0f;
  float descent_ = 0.0f;
  float lineHeight_ = 0.0f;

  // --- CPU アトラス / パッキング ---
  uint32_t atlasSize_ = 0;
  std::vector<uint8_t> cpuAtlas_; ///< R8、atlasSize_ * atlasSize_
  uint32_t shelfX_ = 0;
  uint32_t shelfY_ = 0;
  uint32_t shelfH_ = 0;
  bool atlasFull_ = false;
  bool warnedFull_ = false;

  std::unordered_map<uint32_t, GlyphInfo> glyphs_;

  // --- 転送管理 ---
  uint32_t dirtyMinY_ = UINT32_MAX;
  uint32_t dirtyMaxY_ = 0; ///< inclusive

  // --- GPU ---
  ID3D12Device *device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
  SRVManager::Handle srv_{};

  struct Staging {
    Microsoft::WRL::ComPtr<ID3D12Resource> res;
    uint8_t *mapped = nullptr;
  };
  Staging staging_[kStagingCount]{};
};
