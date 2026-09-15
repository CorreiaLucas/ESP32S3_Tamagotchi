#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <Arduino.h>
#include "HardwareConfig.h"

class SoundManager {
private:
  bool isMuted = true;
public:
  void begin();
  void playClick();
  void playHappyTone();
  void playAlarm();
  void playSadTone();

  void toggleMute() {
    isMuted = !isMuted;
  }
  bool getMuted() const {
    return isMuted;
  }
};

#endif