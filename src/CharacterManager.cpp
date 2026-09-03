#include "CharacterManager.h"
#include "Sprites.h"

CharacterManager::CharacterManager() {
  x = 100;
  oldX = 100;
  y = 145;
  dx = 3;
  spriteWidth = 90;
  spriteHeight = 90;
  currentAction = WALKING;

  lastMoveTime = 0;
  lastFrameTime = 0;
  currentFrame = 0;
  facingRight = true;
}

void CharacterManager::begin() {
}

void CharacterManager::update(DisplayManager& display) {
  uint32_t currentTime = millis();

  if (currentTime - lastFrameTime > 150) {
    lastFrameTime = currentTime;
    currentFrame++;
    if (currentFrame > 3) currentFrame = 0;

    if (currentAction == SLEEPING) {
      if (facingRight) {
        display.drawSprite(x, y, spriteWidth, spriteHeight, sleep_frames[currentFrame]);
      } else {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, sleep_frames[currentFrame]);
      }
    } else if (currentAction == DEAD) {
      display.drawSprite(x, y, spriteWidth, spriteHeight, dead_frame);
    } else if (currentAction == SAD) {
      if (facingRight) {
        display.drawSprite(x, y, spriteWidth, spriteHeight, sad_frames[currentFrame]);
      } else {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, sad_frames[currentFrame]);
      }
    } else if (currentAction == MINIGAME) {
      if (facingRight) {
        display.drawSprite(x, y, spriteWidth, spriteHeight, walk_frames[currentFrame]);
      } else {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, walk_frames[currentFrame]);
      }
    } else if (currentAction == EATING) {
      if (facingRight) {
        display.drawSprite(x, y, spriteWidth, spriteHeight, eat_frames[currentFrame]);
      } else {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, eat_frames[currentFrame]);
      }

      if (currentTime - actionStartTime > 3000) {
        setAction(WALKING);
      }
    } else if (currentAction == PLAYING) {
      if (facingRight) {
        display.drawSprite(x, y, spriteWidth, spriteHeight, play_frames[currentFrame]);
      } else {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, play_frames[currentFrame]);
      }

      if (currentTime - actionStartTime > 3000) {
        setAction(WALKING);
      }
    }
  }

  if (currentTime - lastMoveTime > 30) {
    lastMoveTime = currentTime;

    if (currentAction == WALKING) {
      x += dx;

      if (x <= 0) {
        x = 0;
        dx = -dx;
        facingRight = true;
      } else if (x >= (280 - spriteWidth)) {
        x = 280 - spriteWidth;
        dx = -dx;
        facingRight = false;
      }
      if (facingRight) {
        display.drawSprite(x, y, spriteWidth, spriteHeight, walk_frames[currentFrame]);
      } else {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, walk_frames[currentFrame]);
      }
    }
  }
}

void CharacterManager::setAction(PetAction newAction) {
  if (currentAction != newAction) {
    currentAction = newAction;
    actionStartTime = millis();
    currentFrame = 0;
  }
}