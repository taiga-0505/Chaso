#include "../RenderQueue.h"
#include "../RenderContext.h"
#include <algorithm>

namespace RC {

void RenderQueue::Sort() {
  std::sort(packets_.begin(), packets_.end(),
            [](const RenderPacket &a, const RenderPacket &b) {
              return a.sortKey < b.sortKey;
            });
}

void RenderQueue::Execute(RenderContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (packets_.empty() || !cl) {
    return;
  }

  // ソート済みのパケットを順に実行
  // PSO バインドは各ラムダが自身で行う
  for (auto &pkt : packets_) {
    if (pkt.execute) {
      pkt.execute(cl);
    }
  }
}

} // namespace RC
