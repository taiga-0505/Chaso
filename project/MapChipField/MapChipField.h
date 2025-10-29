#pragma once
#include "Math/MathTypes.h" // Vector3
#include "struct.h"         // Transform
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class MapChipField {
public:
  // ===== タイルIDと属性 =====
  using TileId = uint16_t; // 0=Air, 1=Blockのように拡張可能
  enum : TileId { 
      kAir = 0,
      kBlock = 1,
  };

  enum TileFlag : uint32_t {
    kNone = 0,
    kSolid = 1u << 0,  // 当たりあり（床・壁）
  };

  struct TileDef {
    int model = -1;
    float scale = 1.0f; // ブロックサイズに掛け合わせるローカルスケール
    uint32_t flags = kNone;
  };

  struct Index {
    int x;
    int y;
  };
  struct Rect {
    float left, right, bottom, top;
  };

public:
  // ===== 構築 =====
  void SetBlockSize(float s) { blockSize_ = s; }

  // タイル定義の登録（複数種類に対応）
  void RegisterTileDef(TileId id, const TileDef &def) { tileDefs_[id] = def; }
  const TileDef *FindTileDef(TileId id) const {
    auto it = tileDefs_.find(id);
    return (it == tileDefs_.end()) ? nullptr : &it->second;
  }

  // 2D 配列から設定（行=高さ, 列=幅）。各要素は TileId
  void SetFromArray(int width, int height, const TileId *grid);

  // CSV から読み込み（数値ID, p/P,e/E などのスポーン記号にも対応）
  bool LoadFromCSV(const std::string &csvPath);

  // バッチ描画用インスタンスを組み立て（モデルごとにグルーピング）
  void BuildInstances();

  // 描画（モデルごとに DrawModelBatch）
  void Draw() const;

  // ===== 問い合わせ =====
  int Width() const { return width_; }
  int Height() const { return height_; }
  float BlockSize() const { return blockSize_; }

  TileId Get(Index idx) const;
  bool InBounds(Index idx) const {
    return 0 <= idx.x && idx.x < width_ && 0 <= idx.y && idx.y < height_;
  }
  bool IsSolid(Index idx) const {
    TileId id = Get(idx);
    auto def = FindTileDef(id);
    return def && (def->flags & kSolid);
  }
  bool HasFlag(Index idx, TileFlag f) const {
    TileId id = Get(idx);
    auto def = FindTileDef(id);
    return def && (def->flags & f);
  }

  // インデックス←→ワールド
  Index WorldToIndex(const Vector3 &pos) const;
  Vector3 IndexToCenter(Index idx) const; // z=0
  Rect RectAt(Index idx) const;

  // AABB に重なる“必要タイル”の列挙（solid のみ or 任意フラグ）
  std::vector<Index>
  CollectTilesOverlapping(const Rect &aabb,
                          uint32_t requiredFlags = kSolid) const;
  std::vector<Index> CollectTilesNear(const Vector3 &center, float halfW,
                                      float halfH,
                                      uint32_t requiredFlags = kSolid) const;

  // スポーン取得（CSV使用時）
  const std::optional<Index> &PlayerSpawn() const { return playerSpawn_; }
  const std::vector<Index> &EnemySpawns() const { return enemySpawns_; }

  // バッチインスタンスへのアクセス（デバッグ用）
  const std::unordered_map<int, std::vector<Transform>> &Batches() const {
    return batches_;
  }

private:
  int width_ = 0;
  int height_ = 0;
  float blockSize_ = 1.0f;
  std::vector<TileId> tiles_; // row-major size=width_*height_

  // 描画のためにモデル毎へ分割したインスタンス
  std::unordered_map<int, std::vector<Transform>> batches_; // key=model

  // タイル定義
  std::unordered_map<TileId, TileDef> tileDefs_;

  // スポーン（CSV）
  std::optional<Index> playerSpawn_;
  std::vector<Index> enemySpawns_;

  // 1Dインデックス
  int Idx(int x, int y) const { return y * width_ + x; }
};
