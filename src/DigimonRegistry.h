#ifndef DIGIMON_REGISTRY_H
#define DIGIMON_REGISTRY_H

#include <Arduino.h>

// ==========================================================================
//  DIGIMON REGISTRY  (digivolution core)
//  Each Digimon's sprite set is described by one DigimonSprites entry: a
//  uniform on-screen size plus a frame table (+ frame count) for every action
//  the firmware can play. Digivolution is a single pointer swap:
//      cat.setDigimon(&DIGIMON_gargomon);
//  Actions a Digimon doesn't ship art for fall back to a sensible substitute
//  (handled where the table is populated in DigimonRegistry.cpp), so the
//  firmware can always call any action without a null check.
// ==========================================================================

struct DigimonSprites;   // forward declaration for EvolutionReq

// --------------------------------------------------------------------------
// One possible evolution FROM a Digimon INTO `target`, gated by requirements.
// A requirement of 0 means "no requirement" for that field. The player can
// only digivolve into a target once ALL of its requirements are met.
// --------------------------------------------------------------------------
struct EvolutionReq {
  const DigimonSprites* target;   // Digimon this evolves into
  int minMaxHp;                   // require pet maxHp   >= this (0 = ignore)
  int minAp;                      // require pet ap      >= this (0 = ignore)
  int minDp;                      // require pet dp      >= this (0 = ignore)
  int minAgeDays;                 // require pet age     >= this (0 = ignore)
  int minHappiness;               // require happiness   >= this (0 = ignore)
  int minHunger;                  // require hunger      >= this (0 = ignore)
  int minLevel;                   // require pet level   >= this (0 = ignore)
  int minIntelligence;            // require pet INT     >= this (0 = ignore)
  int minSpeed;                   // require pet SPD     >= this (0 = ignore)
};

// Digimon attribute type. Classic triangle (official): Vaccine beats Virus,
// Virus beats Data, Data beats Vaccine. See typeMultiplier() for the damage
// scaling applied in combat.
enum DigimonType { TYPE_DATA = 0, TYPE_VACCINE = 1, TYPE_VIRUS = 2 };

struct DigimonSprites {
  const char* name;
  int spriteSize;      // size (px) the ACTION art is STORED at (canvas resolution)
  int profileSize;     // square size for the profile sprite (px)
  int realHeightCm;    // lore height (cm). Kept for future use (e.g. background
                       // zoom for very tall Digimon). Does NOT affect sprite
                       // size -- sprites draw 1:1 at their stored spriteSize.
  DigimonType type;    // attribute (Data/Vaccine/Virus) for combat type matchup.

  // Base stats for THIS Digimon. On evolve, the pet keeps the MAX of its
  // current stat vs the new Digimon's base (never lose trained progress, but a
  // stronger base lifts weak stats). Also used to seed a fresh pet.
  int baseMaxHp;
  int baseAp;
  int baseDp;
  int baseIntel;
  int baseSpeed;

  // Possible evolutions FROM this Digimon (may be null / count 0 for a final form).
  const EvolutionReq* evolutions;
  int evolutionCount;

  // Each action: pointer to a frame table + how many frames it has.
  const uint16_t* const* walk;      int walkCount;
  const uint16_t* const* walkBack;  int walkBackCount;
  const uint16_t* const* sleep;     int sleepCount;
  const uint16_t* const* eat;       int eatCount;
  const uint16_t* const* play;      int playCount;
  const uint16_t* const* sad;       int sadCount;
  const uint16_t* const* dead;      int deadCount;
  const uint16_t* const* happy;     int happyCount;
  const uint16_t* const* attack;    int attackCount;

  const uint16_t* profile;          // single frame (may be nullptr)

  // ---- Attack-frame box (COMBAT ONLY) -------------------------------------
  // Attack art on the Digimon World sheets is far larger than the normal
  // poses (e.g. 62x73 vs 22x26), so forcing it into `spriteSize` shrank it to
  // ~37% and destroyed the pixels. Attack frames therefore get their OWN box,
  // stored at their own aspect ratio. They are only ever drawn by
  // drawCombatScene(), where the sprite is static -- so a different size here
  // cannot disturb the wandering pet on the main screen.
  // 0 = not generated yet; callers fall back to a `spriteSize` square.
  int attackW;
  int attackH;
};

// Attack-frame box for `d`, falling back to its uniform square size when the
// sprite set predates per-action attack sizing.
inline int digimonAttackW(const DigimonSprites* d) {
  if (!d) return 0;
  return d->attackW > 0 ? d->attackW : d->spriteSize;
}
inline int digimonAttackH(const DigimonSprites* d) {
  if (!d) return 0;
  return d->attackH > 0 ? d->attackH : d->spriteSize;
}

// Registered Digimon (add one extern per generated <name>Sprites set).
extern const DigimonSprites DIGIMON_terriermon;
extern const DigimonSprites DIGIMON_gargomon;
extern const DigimonSprites DIGIMON_chibomon;
extern const DigimonSprites DIGIMON_dorimon;
extern const DigimonSprites DIGIMON_tsunomon;
extern const DigimonSprites DIGIMON_koromon;
extern const DigimonSprites DIGIMON_kapurimon;
extern const DigimonSprites DIGIMON_kuramon;
extern const DigimonSprites DIGIMON_pagumon;
extern const DigimonSprites DIGIMON_pandamon;
extern const DigimonSprites DIGIMON_tanemon;
extern const DigimonSprites DIGIMON_tokomon;

// Ordered list for iteration / digivolution chains, plus a name lookup.
extern const DigimonSprites* const DIGIMON_ALL[];
extern const int DIGIMON_COUNT;

// Return the registered Digimon with this name, or nullptr if unknown.
const DigimonSprites* digimonByName(const char* name);

// Combat damage multiplier for an attacker's type hitting a defender's type.
// Official triangle: Vaccine>Virus>Data>Vaccine. Advantage x1.5, neutral
// x1.0, disadvantage x0.75.
float typeMultiplier(DigimonType attacker, DigimonType defender);

// Evaluate whether the pet currently meets an evolution's requirements.
bool evolutionRequirementsMet(const EvolutionReq& req,
                              int maxHp, int ap, int dp,
                              int ageDays, int happiness, int hunger,
                              int level, int intelligence, int speed);

#endif
