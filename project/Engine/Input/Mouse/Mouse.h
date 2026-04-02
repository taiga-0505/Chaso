#pragma once
#include "../InputCommon.h"

class Mouse {
public:
    Mouse(IDirectInput8* directInput, HWND hwnd);
    ~Mouse();

    void Update();

    bool IsPressed(int button) const;
    bool IsTrigger(int button) const;
    bool IsRelease(int button) const;

    LONG GetX() const;
    LONG GetY() const;
    LONG GetZ() const;

private:
    Microsoft::WRL::ComPtr<IDirectInputDevice8> device_;
    DIMOUSESTATE state_ = {};
    DIMOUSESTATE preState_ = {};
};
