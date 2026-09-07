#include "CharacterManager.h"
#include "Sprites.h"
#include "DisplayManager.h"

CharacterManager::CharacterManager() {
  x = 20;
  oldX = 20;
  y = 36;
  dx = 2;
  dy = 1;
  spriteWidth = 48;
  spriteHeight = 48;
  currentAction = WALKING;

  nextWanderTime = 0;
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
      if (currentTime >= nextWanderTime) {
        if (random(0, 5) == 0) {
          // occasionally pause, like sniffing around
          dx = 0;
          dy = 0;
        } else {
          dx = random(-2, 3);
          dy = random(-1, 2);
          if (dx == 0 && dy == 0) dx = 1; // never fully stall outside the pause case
        }
        nextWanderTime = currentTime + random(1500, 4000);
      }

      x += dx;
      y += dy;

      if (x <= 0) {
        x = 0;
        dx = -dx + random(-1, 2);
        if (dx <= 0) dx = 1;
        facingRight = true;
      } else if (x >= (SCREEN_WIDTH - spriteWidth)) {
        x = SCREEN_WIDTH - spriteWidth;
        dx = -dx + random(-1, 2);
        if (dx >= 0) dx = -1;
        facingRight = false;
      }

      if (y <= UI_BAR_HEIGHT) {
        y = UI_BAR_HEIGHT;
        dy = -dy + random(-1, 2);
        if (dy <= 0) dy = 1;
      } else if (y >= (SCREEN_HEIGHT - spriteHeight)) {
        y = SCREEN_HEIGHT - spriteHeight;
        dy = -dy + random(-1, 2);
        if (dy >= 0) dy = -1;
      }

      if (facingRight) {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, walk_frames[currentFrame]);
      } else {
        display.drawSprite(x, y, spriteWidth, spriteHeight, walk_frames[currentFrame]);
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