#pragma once
#include "../InputCommon.h"

class Controller {
public:
    Controller();
    ~Controller();

    void Update();

    bool IsConnected() const;
    bool IsButtonPressed(WORD button) const;
    bool IsButtonTrigger(WORD button) const;
    bool IsButtonRelease(WORD button) const;

    SHORT GetThumbLX() const;
    SHORT GetThumbLY() const;
    SHORT GetThumbRX() const;
    SHORT GetThumbRY() const;
    BYTE GetLeftTrigger() const;
    BYTE GetRightTrigger() const;

    void SetVibration(WORD leftMotor, WORD rightMotor);

    void DrawImGui(const char* label = "コントローラー");

private:
    XINPUT_STATE state_ = {};
    XINPUT_STATE preState_ = {};
    
    // ImGui用
    int leftVibration_ = 30000;
    int rightVibration_ = 30000;
};
