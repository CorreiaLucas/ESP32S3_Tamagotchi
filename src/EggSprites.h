// AUTO-GENERATED egg sprites (7 eggs x 3-frame wobble), RGB565 PROGMEM.
// Source: sprites_out/done/egg_<name>{1,2,3}.png, converted at 32x32 via png_to_rgb565 (fit+pad, no upscale).
#ifndef EGG_SPRITES_H
#define EGG_SPRITES_H

#include <Arduino.h>

#define EGG_SPRITE_SIZE   32   // uniform square size for every egg frame
#define EGG_COUNT         7
#define EGG_FRAME_COUNT   3

extern const uint16_t egg_dorimon_0[] PROGMEM;
extern const uint16_t egg_dorimon_1[] PROGMEM;
extern const uint16_t egg_dorimon_2[] PROGMEM;
extern const uint16_t egg_kapurimon_0[] PROGMEM;
extern const uint16_t egg_kapurimon_1[] PROGMEM;
extern const uint16_t egg_kapurimon_2[] PROGMEM;
extern const uint16_t egg_koromon_0[] PROGMEM;
extern const uint16_t egg_koromon_1[] PROGMEM;
extern const uint16_t egg_koromon_2[] PROGMEM;
extern const uint16_t egg_pagumon_0[] PROGMEM;
extern const uint16_t egg_pagumon_1[] PROGMEM;
extern const uint16_t egg_pagumon_2[] PROGMEM;
extern const uint16_t egg_tanemon_0[] PROGMEM;
extern const uint16_t egg_tanemon_1[] PROGMEM;
extern const uint16_t egg_tanemon_2[] PROGMEM;
extern const uint16_t egg_tokomon_0[] PROGMEM;
extern const uint16_t egg_tokomon_1[] PROGMEM;
extern const uint16_t egg_tokomon_2[] PROGMEM;
extern const uint16_t egg_tsunomon_0[] PROGMEM;
extern const uint16_t egg_tsunomon_1[] PROGMEM;
extern const uint16_t egg_tsunomon_2[] PROGMEM;

extern const uint16_t* const egg_dorimon_frames[3];
extern const uint16_t* const egg_kapurimon_frames[3];
extern const uint16_t* const egg_koromon_frames[3];
extern const uint16_t* const egg_pagumon_frames[3];
extern const uint16_t* const egg_tanemon_frames[3];
extern const uint16_t* const egg_tokomon_frames[3];
extern const uint16_t* const egg_tsunomon_frames[3];

// Parallel arrays for iterating eggs in the carousel: EGG_FRAMES[i] is the
// 3-frame table for egg i, EGG_STARTER_NAMES[i] the Digimon it hatches.
extern const uint16_t* const* const EGG_FRAMES[EGG_COUNT];
extern const char* const EGG_STARTER_NAMES[EGG_COUNT];

// Reverse lookup: the 3-frame egg table for the Digimon species that hatches
// from it (by name, matching DIGIMON_<name>). Returns nullptr if unknown.
const uint16_t* const* eggFramesForSpecies(const char* speciesName);

#endif
