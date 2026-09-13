#include "InputManager.h"

InputManager::InputManager()
  : lastDebounceTime(0) {}

void InputManager::begin() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
}

bool InputManager::isAnyPressed() {
  return digitalRead(BTN_LEFT) == LOW ||
         digitalRead(BTN_OK) == LOW ||
         digitalRead(BTN_RIGHT) == LOW;
}

bool InputManager::readButton(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      lastDebounceTime = millis();
      return true;
    }
  }
  return false;
}

bool InputManager::isLeftPressed() {
  return readButton(BTN_LEFT);
}
bool InputManager::isOkPressed() {
  return readButton(BTN_OK);
}
bool InputManager::isRightPressed() {
  return readButton(BTN_RIGHT);
}