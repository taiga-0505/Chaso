#include "Fade.h"
#include <algorithm>

void Fade::Init(ID3D12Device* device, DescriptorHeap* srvHeap, float width, float height) {
	// TextureManager 初期化
	texMgr_.Init(device, srvHeap);

	// fade
	fadeSprite_ = std::make_unique<Sprite2D>();
	fadeSprite_->Initialize(device, width, height);
	tx_fade_ = texMgr_.LoadID("Resources/white1x1.png", true);
	fadeSprite_->SetTexture(texMgr_.GetSrv(tx_fade_));
	fadeSprite_->SetSize(width, height);
	fadeSprite_->T().translation = { 0, 0, 0 };
	fadeSprite_->SetColor(Vector4(1, 1, 1, 1));
}

void Fade::Start(Status status, float duration, float alpha) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;
	overlayAlpha_ = alpha; // アルファ値を保存

	switch (status_) {
	case Status::FadeIn:
		fadeSprite_->SetColor(Vector4(0, 0, 0, 1));
		break;
	case Status::FadeOut:
		fadeSprite_->SetColor(Vector4(0, 0, 0, 0));
		break;
	case Status::kOverlay:
		fadeSprite_->SetColor(Vector4(0, 0, 0, overlayAlpha_)); // 保存したアルファ値を使用
		break;
	}
}

void Fade::Stop() {
	status_ = Status::None;
}

void Fade::Update() {
	switch (status_) {
	case Status::None:
		// 何もしない
		break;
	case Status::FadeIn:
		// １フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 0.0fから1.0fの間で、経過時間がフェード継続時間に近くほどアルファ値を大きくする
		fadeSprite_->SetColor(Vector4(0, 0, 0, std::clamp(1.0f - counter_ / duration_, 0.0f, 1.0f)));
		break;
	case Status::FadeOut:
		// １フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 0.0fから1.0fの間で、経過時間がフェード継続時間に近くほどアルファ値を大きくする
		fadeSprite_->SetColor(Vector4(0, 0, 0, std::clamp(counter_ / duration_, 0.0f, 1.0f)));
		break;
	case Status::kOverlay:
		// アルファ値を薄暗い状態に固定
		fadeSprite_->SetColor(Vector4(0, 0, 0, overlayAlpha_));
		break;
	}
	if (fadeSprite_) {
		fadeSprite_->Update();
	}
}

void Fade::Draw(ID3D12GraphicsCommandList* cl) {
	if (fadeSprite_) {
		fadeSprite_->Draw(cl);
	}
}

bool Fade::IsFinished() const {
	// フェード状態による分岐
	switch (status_) {
	case Status::FadeIn:
	case Status::FadeOut:
		if (counter_ >= duration_) {
			return true;
		} else {
			return false;
		}
	}

	return true;
}