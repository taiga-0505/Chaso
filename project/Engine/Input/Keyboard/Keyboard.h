#pragma once
#include <cstdint>
#include "../InputCommon.h"

class Keyboard {
public:
    Keyboard(IDirectInput8* directInput, HWND hwnd);
    ~Keyboard();

    void Update();

    bool IsPressed(uint8_t keyCode) const;
    bool IsKeyUp(uint8_t keyCode) const;
    bool IsTrigger(uint8_t keyCode) const;
    bool IsRelease(uint8_t keyCode) const;

private:
    Microsoft::WRL::ComPtr<IDirectInputDevice8> device_;
    BYTE key_[256] = {};
    BYTE preKey_[256] = {};
};
