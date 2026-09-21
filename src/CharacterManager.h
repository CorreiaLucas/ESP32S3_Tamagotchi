#ifndef CHARACTER_MANAGER_H
#define CHARACTER_MANAGER_H

#include "DisplayManager.h"
#include "DigimonRegistry.h"

enum PetAction { IDLE,
                 WALKING,
                 EATING,
                 SLEEPING,
                 PLAYING,
                 DEAD,
                 SAD,
                 HAPPY,
                 MINIGAME };

class CharacterManager {
private:
  int x, y;
  int oldX;
  int oldY;
  int dx;
  int dy;
  int spriteWidth;
  int spriteHeight;
  PetAction currentAction;

  uint32_t lastMoveTime;
  uint32_t lastFrameTime;
  uint32_t actionStartTime;
  uint32_t nextWanderTime;
  
  int currentFrame;
  bool facingRight;

  // While WALKING with dx==dy==0 the pet is standing still. Instead of cycling
  // the walk animation on the spot (which read as "walking in place"), it holds
  // ONE still pose, re-picked at the start of each pause.
  const uint16_t* idlePose = nullptr;
  // Pick the still pose: walk1 or happy1 (== index 0 of those tables, since
  // walk1.png generates walk_0). Falls back safely if a table is missing.
  const uint16_t* pickIdlePose() const;
  bool isStandingStill() const { return currentAction == WALKING && dx == 0 && dy == 0; }

  // Active Digimon sprite set. Digivolution swaps this pointer.
  const DigimonSprites* digimon = nullptr;

  // Frame table + count for the CURRENT action of the active Digimon.
  const uint16_t* const* actionFrames(int& countOut) const;

public:
  CharacterManager();
  void begin();
  void update(DisplayManager& display);
  void setAction(PetAction newAction);
  // Set the active Digimon (digivolution). Updates sprite size too.
  void setDigimon(const DigimonSprites* d);
  const DigimonSprites* getDigimon() const { return digimon; }

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
    if (newX > SCREEN_WIDTH - spriteWidth) newX = SCREEN_WIDTH - spriteWidth;
    x = newX;
  }
  void setFacingRight(bool right) {
    facingRight = right;
  }

  int getWidth() const { return spriteWidth; }
  int getHeight() const { return spriteHeight; }
  bool getFacingRight() const { return facingRight; }

  // Return the sprite frame pointer for the CURRENT action + animation frame.
  // Used by the buffered main-scene renderer so the pet can be composited in
  // a single blit (no waiting for the next animation tick). Declared here,
  // defined in CharacterManager.cpp (needs Sprites.h).
  const uint16_t* getCurrentFrame() const;

  // Whether the current frame should be drawn horizontally mirrored, matching
  // the flip logic used in update().
  bool getCurrentFlip() const {
    // WALKING draws flipped when facingRight (see update()); other actions
    // draw flipped when !facingRight.
    if (currentAction == WALKING) return facingRight;
    return !facingRight;
  }
};

#endif