#pragma once

// ImGuiの有効・無効を切り替えるマクロ
// デバッグまたは開発ビルドの場合に有効にする
#ifndef RC_ENABLE_IMGUI
#if defined(_DEBUG) || defined(RC_DEVELOPMENT)
#define RC_ENABLE_IMGUI 1
#else
#define RC_ENABLE_IMGUI 0
#endif
#endif
