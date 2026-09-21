#include "CharacterManager.h"
#include "Sprites.h"
#include "DisplayManager.h"

// ==========================================================================
//  WANDER TUNING  (how restless the pet is)
//
//  Previously the pet moved ~80% of the time at up to 2px every 30ms (~66
//  px/s), so it crossed the screen in about two seconds and effectively never
//  stood still. These constants make it mostly idle, drifting slowly.
//
//  Raise WANDER_PAUSE_PERCENT for a calmer pet; raise MOVE_TICK_MS or lower
//  WANDER_STEP_MAX to slow it down.
// ==========================================================================
static const uint32_t MOVE_TICK_MS         = 80;   // was 30 -> slower travel
                                                   // speed = 1000/TICK * STEP px/s
static const int      WANDER_PAUSE_PERCENT = 65;   // was 20 -> mostly still
static const int      WANDER_STEP_MAX      = 1;    // was 2  -> gentler steps
static const int      VERTICAL_CHANCE      = 3;    // 1-in-N intervals drift in y
static const uint32_t PAUSE_MIN_MS         = 2500; // stand still this long
static const uint32_t PAUSE_MAX_MS         = 6000;
static const uint32_t MOVE_MIN_MS          = 700;  // and walk only this long
static const uint32_t MOVE_MAX_MS          = 1800;

// Keep a step within the configured maximum. The edge-bounce code in update()
// re-derives dx/dy as `-dx + random(-1,2)`, which can yield +/-2 -- double the
// intended top speed -- so every bounce gets clamped back to WANDER_STEP_MAX.
// Clamping only shrinks the magnitude, so a moving step can never become 0
// (which would be mistaken for "standing still").
static int clampStep(int v, int maxAbs) {
  if (v >  maxAbs) return  maxAbs;
  if (v < -maxAbs) return -maxAbs;
  return v;
}


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

    // WALKING while standing still: hold ONE pose (walk1 / happy1) rather than
    // cycling the walk cycle on the spot. Redrawn on this ~150ms tick (not the
    // movement tick) so the stat-bar chrome can never leave it half-painted.
    if (currentAction == WALKING && dx == 0 && dy == 0) {
      if (!idlePose) idlePose = pickIdlePose();
      if (idlePose) {
        // Same flip convention as the walking draw below.
        if (facingRight) {
          display.drawSpriteFlipped(x, y, spriteWidth, spriteHeight, idlePose);
        } else {
          display.drawSprite(x, y, spriteWidth, spriteHeight, idlePose);
        }
      }
    }

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

  if (currentTime - lastMoveTime > MOVE_TICK_MS) {
    lastMoveTime = currentTime;

    if (currentAction == WALKING && digimon) {
      // Remember where the sprite currently sits so we can erase its whole
      // bounding box after it moves (clears BOTH x and y trails, any size).
      int prevX = x;
      int prevY = y;
      if (currentTime >= nextWanderTime) {
        if (random(0, 100) < WANDER_PAUSE_PERCENT) {
          // Stand still for a good while and hold ONE pose.
          dx = 0;
          dy = 0;
          idlePose = pickIdlePose();
          nextWanderTime = currentTime + random(PAUSE_MIN_MS, PAUSE_MAX_MS);
        } else {
          // Short, slow stroll. Vertical drift only occasionally, so movement
          // reads as walking rather than floating around.
          dx = random(-WANDER_STEP_MAX, WANDER_STEP_MAX + 1);
          dy = (random(0, VERTICAL_CHANCE) == 0) ? random(-1, 2) : 0;
          if (dx == 0 && dy == 0) dx = 1;   // a stroll must actually move
          idlePose = nullptr;               // back to the walk cycle
          nextWanderTime = currentTime + random(MOVE_MIN_MS, MOVE_MAX_MS);
        }
      }

      // Standing still: nothing to move, erase or re-blit -- the idle pose is
      // drawn on the animation tick above. Returning BEFORE the edge-bounce
      // code matters: that code force-sets dx when the pet is against a wall,
      // which would otherwise cut a pause short.
      if (dx == 0 && dy == 0) {
        return;
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

      // The four bounces above can overshoot the configured step size, which is
      // what made the pet occasionally sprint after touching a wall.
      dx = clampStep(dx, WANDER_STEP_MAX);
      dy = clampStep(dy, WANDER_STEP_MAX);

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
    idlePose = nullptr;      // re-pick on the next standing-still interval
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

// "walk1" / "happy1" are index 0 of their tables (walk1.png -> walk_0).
// Randomly alternate between them so pauses don't always look identical.
const uint16_t* CharacterManager::pickIdlePose() const {
  if (!digimon) return nullptr;
  bool preferHappy = (random(0, 2) == 1);
  if (preferHappy && digimon->happy && digimon->happyCount > 0) return digimon->happy[0];
  if (digimon->walk && digimon->walkCount > 0)                  return digimon->walk[0];
  if (digimon->happy && digimon->happyCount > 0)                return digimon->happy[0];
  return nullptr;
}

const uint16_t* CharacterManager::getCurrentFrame() const {
  // While standing still report the held pose, so a full-scene repaint
  // (menu exit, screensaver wake) matches what is already on the panel.
  if (isStandingStill() && idlePose) return idlePose;

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
  // Stale pose would point into the previous Digimon's sprite table while the
  // draw size switches to the new one -> out-of-bounds read. Always clear it.
  idlePose = nullptr;
  if (d) {
    // Draw at the stored art size, 1:1 (no runtime scaling).
    spriteWidth = d->spriteSize;
    spriteHeight = d->spriteSize;
  }
  currentFrame = 0;
}
