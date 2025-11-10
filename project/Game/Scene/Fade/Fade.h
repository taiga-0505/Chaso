#pragma once

#include <memory>
#include "Dx12Core.h"
#include "Sprite2D/Sprite2D.h"
#include "Texture/TextureManager/TextureManager.h"

class Fade {
public: // メンバ変数

	// フェードの状態
	enum class Status {
		None,     // フェードなし
		FadeIn,   // フェードイン中
		FadeOut,  // フェードアウト中
		kOverlay, // 薄暗いオーバーレイ
	};

	// 現在のフェードの状態
	Status status_ = Status::None;

public: // メンバ関数
	// 初期化
	void Init(ID3D12Device* device, DescriptorHeap* srvHeap, float width, float height);

	// フェード開始
	void Start(Status status, float duration, float alpha = 0.5f); // アルファ値を追加
	// フェード終了
	void Stop();

	// 更新
	void Update();
	// 描画
	void Draw(ID3D12GraphicsCommandList* cl);

	// フェード終了判定
	bool IsFinished() const;

private: // メンバ変数
	// テクスチャ
	TextureManager texMgr_;

	// fade
	std::unique_ptr<Sprite2D> fadeSprite_ = nullptr;
	int tx_fade_ = -1;

	// フェードの持続時間
	float duration_ = 0.0f;
	// 経過時間カウンター
	float counter_ = 0.0f;
	// 薄暗いオーバーレイのアルファ値
	float overlayAlpha_ = 0.0f;

};
