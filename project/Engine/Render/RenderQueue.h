#pragma once

// ============================================================================
// RenderQueue
// ----------------------------------------------------------------------------
// 描画コマンドをソートキーで並べ替えてから一括実行する仕組み。
// 同じ PSO / テクスチャのコマンドが連続するようにソートし、
// ステート変更回数を最小化する。
//
// 既存の PushCommand3D（ラムダ方式）と並行運用可能。
// RenderModel 系の描画のみ段階的に RenderQueue に移行する。
// ============================================================================

#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <string_view>
#include <vector>

#include "Math/MathTypes.h"
#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode

namespace RC {

class RenderContext; // 前方宣言

// ── ソートキー構成（64bit）──
// [63..56] レイヤー：不透明=0, 半透明=1, ガラス=2
// [55..40] PSO ID（パイプライン種別のハッシュ）
// [39..24] テクスチャ ID（SRV ptr の下位ビット）
// [23..0]  深度（front-to-back / back-to-front）
namespace SortKey {

enum Layer : uint8_t {
  kLayerOpaque = 0,
  kLayerTranslucent = 1,
  kLayerGlass = 2,
};

/// PSO prefix 文字列 → 16bit ハッシュ
inline uint16_t HashPSO(std::string_view prefix) {
  // FNV-1a 32bit → 上位16bitと下位16bitを XOR fold
  uint32_t h = 2166136261u;
  for (char c : prefix) {
    h ^= static_cast<uint32_t>(c);
    h *= 16777619u;
  }
  return static_cast<uint16_t>((h >> 16) ^ (h & 0xFFFF));
}

/// SRV の GPU ハンドルから 16bit ハッシュを作る
inline uint16_t HashTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) {
  // ptr の下位ビットをXOR fold
  uint64_t v = srv.ptr;
  return static_cast<uint16_t>((v >> 16) ^ (v & 0xFFFF));
}

/// ソートキー組み立て
inline uint64_t Make(Layer layer, uint16_t psoHash, uint16_t texHash,
                     uint32_t depth24 = 0) {
  return (uint64_t(layer) << 56) | (uint64_t(psoHash) << 40) |
         (uint64_t(texHash) << 24) | uint64_t(depth24 & 0x00FFFFFF);
}

} // namespace SortKey

// ── RenderPacket ──
// ソート可能な描画単位。実行時にラムダを呼ぶ方式で、
// 既存コードとの互換性を保ちつつステート変更のグルーピングを実現する。
struct RenderPacket {
  uint64_t sortKey = 0;

  // PSO binding 情報（同じ PSO なら切替をスキップ）
  std::string_view psoPrefix; // "object3d", "object3d_inst" 等
  BlendMode blend = kBlendModeNone;

  // 実行コールバック（PSO/Camera/Lights バインド完了後に呼ばれる）
  std::function<void(ID3D12GraphicsCommandList *)> execute;
};

// ── RenderQueue ──
class RenderQueue {
public:
  /// パケットをキューに追加
  void Push(RenderPacket &&packet) {
    packets_.push_back(std::move(packet));
  }

  /// ソートキーでソート
  void Sort();

  /// 全パケットを実行（PSO の重複切替を最小化）
  void Execute(RenderContext &ctx, ID3D12GraphicsCommandList *cl);

  /// キューをクリア
  void Clear() { packets_.clear(); }

  /// パケット数
  size_t Size() const { return packets_.size(); }

  /// 空かどうか
  bool Empty() const { return packets_.empty(); }

private:
  std::vector<RenderPacket> packets_;
};

} // namespace RC
