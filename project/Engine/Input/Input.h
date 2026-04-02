#pragma once
#include "EngineConfig.h"
#include <memory>
#include "InputCommon.h"

// 各デバイスクラスの前方宣言
class Keyboard;
class Mouse;
class Controller;

class Input {
public:
    Input(HWND hwnd);
    ~Input();

    void Update();

    // デバイスへのアクセス
    Keyboard* GetKeyboard() const { return keyboard_.get(); }
    Mouse* GetMouse() const { return mouse_.get(); }
    Controller* GetController() const { return controller_.get(); }

    // ============================
    // キーボード入力 (互換性維持用)
    // ============================
    bool IsKeyPressed(uint8_t keyCode) const;
    bool IsKeyUp(uint8_t keyCode) const;
    bool IsKeyTrigger(uint8_t keyCode) const;
    bool IsKeyRelease(uint8_t keyCode) const;

    // ============================
    // マウス入力 (互換性維持用)
    // ============================
    LONG GetMouseX() const;
    LONG GetMouseY() const;
    LONG GetMouseZ() const;
    bool IsMousePressed(int button) const;
    bool IsMouseTrigger(int button) const;
    bool IsMouseRelease(int button) const;

    // ============================
    // コントローラー入力 (互換性維持用)
    // ============================
    bool IsXInputConnected() const;
    bool IsXInputButtonPressed(WORD button) const;
    bool IsXInputButtonTrigger(WORD button) const;
    bool IsXInputButtonRelease(WORD button) const;
    SHORT GetXInputThumbLX() const;
    SHORT GetXInputThumbLY() const;
    SHORT GetXInputThumbRX() const;
    SHORT GetXInputThumbRY() const;
    BYTE GetXInputLeftTrigger() const;
    BYTE GetXInputRightTrigger() const;
    void SetXInputVibration(WORD leftMotor, WORD rightMotor);
    void ControllerImGui(const char* label = "コントローラー");

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Mouse> mouse_;
    std::unique_ptr<Controller> controller_;
};
