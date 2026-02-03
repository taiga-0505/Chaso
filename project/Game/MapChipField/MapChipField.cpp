#include "MapChipField.h"
#include "RenderCommon.h"
#include <cassert>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

using namespace RC;

void MapChipField::Update(float dt) {
  for (const auto &[model, speed] : spinYSpeedByModel_) {
    auto it = batches_.find(model);
    if (it == batches_.end())
      continue;
    for (auto &t : it->second) {
      t.rotation.y += speed * dt;
    }
  }
}

void MapChipField::SetFromArray(int width, int height, const TileId *grid) {
  width_ = width;
  height_ = height;
  tiles_.assign(width_ * height_, kAir);

  for (int srcY = 0; srcY < height_; ++srcY) {
    const int y = (height_ - 1) - srcY; // ★
    for (int x = 0; x < width_; ++x) {
      tiles_[Idx(x, y)] = grid[srcY * width_ + x];
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
  coinSpawns_.clear();

  for (int csvY = 0; csvY < height_; ++csvY) {
    const int y =
        (height_ - 1) - csvY; // CSVは上が0行目 → 内部は下が0になるよう反転
    assert((int)rows[csvY].size() == width_);

    for (int x = 0; x < width_; ++x) {
      std::string w = rows[csvY][x];

      if (!w.empty()) {
        char c = (char)std::tolower((unsigned char)w[0]);
        if (c == 'p') {
          playerSpawn_ = Index{x, y}; // ここも反転後のyで保存
          tiles_[Idx(x, y)] = kAir;
          continue;
        }
        if (c == 'e') {
          enemySpawns_.push_back(Index{x, y});
          tiles_[Idx(x, y)] = kAir;
          continue;
        }
      }

      TileId id = (TileId)std::atoi(w.c_str());
      tiles_[Idx(x, y)] = id; // 反転後のyへ格納

      // 2 は「コイン配置」扱い：タイルとしては置かず、スポーン座標だけ保存
      if (id == kCoin) {
        coinSpawns_.push_back(Index{x, y});
        tiles_[Idx(x, y)] = kAir;
      } else if (id == kGoal) {
        goalSpawns_.push_back(Index{x, y});
        tiles_[Idx(x, y)] = kAir;
      } else {
        tiles_[Idx(x, y)] = id;
        if (id == kBlock) {
          blockSpawns_.push_back(Index{x, y});
        }
      }
    }
  }
  return true;
}

void MapChipField::BuildInstances() {
  batches_.clear();
  spinYSpeedByModel_.clear();
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
      t.translation = {(float)x * blockSize_, (float)y * blockSize_, 0.f};
      batches_[def->model].push_back(t);

      if (def->flags & kSpinY) {
        spinYSpeedByModel_[def->model] = def->spinSpeedY;
      }
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
  const float inv = 1.0f / blockSize_;
  auto ToCenterIndex = [&](float p) -> int {
    return (int)std::floor(p * inv + 0.5f);
  };

  const int x0 = ToCenterIndex(aabb.left);
  const int x1 = ToCenterIndex(aabb.right);
  const int y0 = ToCenterIndex(aabb.bottom);
  const int y1 = ToCenterIndex(aabb.top);

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
