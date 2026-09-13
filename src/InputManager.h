#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "HardwareConfig.h"

class InputManager {
private:
    uint32_t lastDebounceTime;
    const uint32_t debounceDelay = 200;
    bool readButton(uint8_t pin);

public:
    InputManager();
    void begin();
    bool isAnyPressed();
    bool isLeftPressed();
    bool isOkPressed();
    bool isRightPressed();
};

#endif