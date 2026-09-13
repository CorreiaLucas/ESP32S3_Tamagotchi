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
  // Default starter Digimon. Digivolution later calls setDigimon().
  if (!digimon) setDigimon(&DIGIMON_terriermon);
}

void CharacterManager::update(DisplayManager& display) {
  uint32_t currentTime = millis();

  if (currentTime - lastFrameTime > 150 && digimon) {
    lastFrameTime = currentTime;

    // Advance the animation within the CURRENT action's frame count (actions
    // no longer assume exactly 4 frames -- each Digimon action can differ).
    int count = 1;
    const uint16_t* const* frames = actionFrames(count);
    if (count < 1) count = 1;
    currentFrame++;
    if (currentFrame >= count) currentFrame = 0;

    // WALKING is drawn in the movement block below; here we handle the
    // stationary/animated actions. (MINIGAME uses walk frames, drawn here.)
    if (currentAction != WALKING) {
      const uint16_t* frame = frames[currentFrame];
      // Flip convention: WALKING flips when facingRight; every other action
      // flips when !facingRight (matches the original per-action logic).
      bool flip = (!facingRight);
      if (flip) {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, frame);
      } else {
        display.drawSprite(x, y, spriteWidth, spriteHeight, frame);
      }

      // EATING / PLAYING auto-return to walking after 3s.
      if ((currentAction == EATING || currentAction == PLAYING ||
           currentAction == HAPPY) &&
          currentTime - actionStartTime > 3000) {
        setAction(WALKING);
      }
    }
  }

  if (currentTime - lastMoveTime > 30) {
    lastMoveTime = currentTime;

    if (currentAction == WALKING && digimon) {
      // Remember where the sprite currently sits so we can erase its whole
      // bounding box after it moves (clears BOTH x and y trails, any size).
      int prevX = x;
      int prevY = y;
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
      
      // Choose walk vs walk-back table from the active Digimon, clamped to
      // each table's own frame count.
      const uint16_t* const* tbl = (dy < 0) ? digimon->walkBack : digimon->walk;
      int cnt = (dy < 0) ? digimon->walkBackCount : digimon->walkCount;
      if (cnt < 1) cnt = 1;
      int wf = currentFrame % cnt;
      const uint16_t* activeWalkFrame = tbl[wf];

      // Erase ONLY the strip the sprite vacated (old box minus new box), then
      // draw the new frame. Clearing just the margin -- not the whole old box --
      // means the pixels under the pet are never blanked, so it doesn't blink.
      if (prevX != x || prevY != y) {
        display.clearSpriteMargin(prevX, prevY, x, y, spriteWidth, spriteHeight);
      }

      if (facingRight) {
        display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, activeWalkFrame);
      } else {
        display.drawSprite(x, y, spriteWidth, spriteHeight, activeWalkFrame);
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


// Frame table + count for the current action of the ACTIVE Digimon.
const uint16_t* const* CharacterManager::actionFrames(int& countOut) const {
  countOut = 1;
  if (!digimon) return nullptr;
  switch (currentAction) {
    case SLEEPING: countOut = digimon->sleepCount;   return digimon->sleep;
    case DEAD:     countOut = digimon->deadCount;    return digimon->dead;
    case SAD:      countOut = digimon->sadCount;     return digimon->sad;
    case EATING:   countOut = digimon->eatCount;     return digimon->eat;
    case HAPPY:    countOut = digimon->happyCount;   return digimon->happy;
    case PLAYING:  countOut = digimon->playCount;    return digimon->play;
    case MINIGAME: countOut = digimon->walkCount;    return digimon->walk;
    case WALKING:
    default:       countOut = digimon->walkCount;    return digimon->walk;
  }
}

const uint16_t* CharacterManager::getCurrentFrame() const {
  int count = 1;
  const uint16_t* const* frames = actionFrames(count);
  if (!frames || count < 1) return nullptr;
  int f = currentFrame;
  if (f < 0) f = 0;
  if (f >= count) f = f % count;
  return frames[f];
}

void CharacterManager::setDigimon(const DigimonSprites* d) {
  digimon = d;
  if (d) {
    // Draw at the stored art size, 1:1 (no runtime scaling).
    spriteWidth = d->spriteSize;
    spriteHeight = d->spriteSize;
  }
  currentFrame = 0;
}
