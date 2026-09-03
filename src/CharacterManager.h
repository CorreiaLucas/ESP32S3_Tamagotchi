#ifndef CHARACTER_MANAGER_H
#define CHARACTER_MANAGER_H

#include "DisplayManager.h"

enum PetAction { IDLE,
                 WALKING,
                 EATING,
                 SLEEPING,
                 PLAYING,
                 DEAD,
                 SAD,
                 MINIGAME };

class CharacterManager {
private:
  int x, y;
  int oldX;
  int dx;
  int spriteWidth;
  int spriteHeight;
  PetAction currentAction;

  uint32_t lastMoveTime;
  uint32_t lastFrameTime;
  uint32_t actionStartTime;

  int currentFrame;
  bool facingRight;

public:
  CharacterManager();
  void begin();
  void update(DisplayManager& display);
  void setAction(PetAction newAction);

  PetAction getCurrentAction() const {
    return currentAction;
  }

  int getX() const {
    return x;
  }
  int getOldX() const {
    return oldX;
  }
  int getY() const {
    return y;
  }
  void setX(int newX) {
    if (newX < 0) newX = 0;
    if (newX > 280 - spriteWidth) newX = 280 - spriteWidth;
    x = newX;
  }
  void setFacingRight(bool right) {
    facingRight = right;
  }
};

#endif