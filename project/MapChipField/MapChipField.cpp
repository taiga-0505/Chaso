#include "MapChipField.h"
#include "RenderCommon.h"
#include <cassert>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

void MapChipField::SetFromArray(int width, int height, const TileId *grid) {
  width_ = width;
  height_ = height;
  tiles_.assign(width_ * height_, kAir);
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      tiles_[Idx(x, y)] = grid[y * width_ + x];
    }
  }
}

bool MapChipField::LoadFromCSV(const std::string &csvPath) {
  std::ifstream ifs(csvPath);
  if (!ifs.is_open())
    return false;

  std::vector<std::vector<std::string>> rows;
  std::string line;
  while (std::getline(ifs, line)) {
    std::stringstream ss(line);
    std::string cell;
    std::vector<std::string> row;
    while (std::getline(ss, cell, ','))
      row.push_back(cell);
    if (!row.empty())
      rows.push_back(std::move(row));
  }
  ifs.close();
  if (rows.empty())
    return false;

  height_ = (int)rows.size();
  width_ = (int)rows[0].size();
  tiles_.assign(width_ * height_, kAir);
  playerSpawn_.reset();
  enemySpawns_.clear();

  for (int y = 0; y < height_; ++y) {
    assert((int)rows[y].size() == width_);
    for (int x = 0; x < width_; ++x) {
      std::string w = rows[y][x];
      if (!w.empty()) {
        char c = (char)std::tolower((unsigned char)w[0]);
        if (c == 'p') {
          playerSpawn_ = Index{x, y};
          tiles_[Idx(x, y)] = kAir;
          continue;
        }
        if (c == 'e') {
          enemySpawns_.push_back(Index{x, y});
          tiles_[Idx(x, y)] = kAir;
          continue;
        }
      }
      // 数値IDとして扱う
      TileId id = (TileId)std::atoi(w.c_str());
      tiles_[Idx(x, y)] = id;
    }
  }
  return true;
}

void MapChipField::BuildInstances() {
  batches_.clear();
  // すべてのタイルを走査し、モデルごとに Transform を詰める
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      TileId id = tiles_[Idx(x, y)];
      if (id == kAir)
        continue;
      const TileDef *def = FindTileDef(id);
      if (!def || def->model < 0)
        continue; // モデル未設定は描画しない

      Transform t{};
      float s = blockSize_ * (def->scale <= 0.f ? 1.f : def->scale);
      t.scale = {s, s, s};
      t.rotation = {0.f, 0.f, 0.f};
      t.translation = {(float)x, (float)y, 0.f};
      batches_[def->model].push_back(t);
    }
  }
}

void MapChipField::Draw() const {
  for (const auto &[model, inst] : batches_) {
    if (!inst.empty())
      RC::DrawModelBatch(model, inst);
  }
}

MapChipField::TileId MapChipField::Get(Index idx) const {
  if (!InBounds(idx))
    return kAir;
  return tiles_[Idx(idx.x, idx.y)];
}

MapChipField::Index MapChipField::WorldToIndex(const Vector3 &pos) const {
  int x = (int)std::floor(pos.x / blockSize_ + 0.5f);
  int y = (int)std::floor(pos.y / blockSize_ + 0.5f);
  return {x, y};
}

Vector3 MapChipField::IndexToCenter(Index idx) const {
  return Vector3{(idx.x + 0.0f) * blockSize_, (idx.y + 0.0f) * blockSize_,
                 0.0f};
}

MapChipField::Rect MapChipField::RectAt(Index idx) const {
  const Vector3 c = IndexToCenter(idx);
  const float h = blockSize_ * 0.5f;
  return Rect{c.x - h, c.x + h, c.y - h, c.y + h};
}

std::vector<MapChipField::Index>
MapChipField::CollectTilesOverlapping(const Rect &aabb,
                                      uint32_t requiredFlags) const {
  if (width_ == 0 || height_ == 0)
    return {};
  const int x0 = (int)std::floor((aabb.left) / blockSize_);
  const int x1 = (int)std::floor((aabb.right) / blockSize_);
  const int y0 = (int)std::floor((aabb.bottom) / blockSize_);
  const int y1 = (int)std::floor((aabb.top) / blockSize_);

  std::vector<Index> out;
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      Index idx{x, y};
      if (!InBounds(idx))
        continue;
      TileId id = Get(idx);
      const TileDef *def = FindTileDef(id);
      if (!def)
        continue;
      if ((def->flags & requiredFlags) == requiredFlags)
        out.push_back(idx);
    }
  }
  return out;
}

std::vector<MapChipField::Index>
MapChipField::CollectTilesNear(const Vector3 &center, float halfW, float halfH,
                               uint32_t requiredFlags) const {
  Rect r{center.x - halfW, center.x + halfW, center.y - halfH,
         center.y + halfH};
  return CollectTilesOverlapping(r, requiredFlags);
}
