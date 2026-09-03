#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>

extern const uint16_t walk_0[] PROGMEM;
extern const uint16_t walk_1[] PROGMEM;
extern const uint16_t walk_2[] PROGMEM;
extern const uint16_t walk_3[] PROGMEM;

extern const uint16_t sleep_0[] PROGMEM;
extern const uint16_t sleep_1[] PROGMEM;
extern const uint16_t sleep_2[] PROGMEM;
extern const uint16_t sleep_3[] PROGMEM;

extern const uint16_t eat_0[] PROGMEM;
extern const uint16_t eat_1[] PROGMEM;
extern const uint16_t eat_2[] PROGMEM;
extern const uint16_t eat_3[] PROGMEM;

extern const uint16_t play_0[] PROGMEM;
extern const uint16_t play_1[] PROGMEM;
extern const uint16_t play_2[] PROGMEM;
extern const uint16_t play_3[] PROGMEM;

extern const uint16_t sad_0[] PROGMEM;
extern const uint16_t sad_1[] PROGMEM;
extern const uint16_t sad_2[] PROGMEM;
extern const uint16_t sad_3[] PROGMEM;

extern const uint16_t dead_frame[] PROGMEM;
extern const uint16_t poop_frame[] PROGMEM;
extern const uint16_t treat_frame[] PROGMEM;

extern const uint16_t* const walk_frames[4];
extern const uint16_t* const sleep_frames[4];
extern const uint16_t* const eat_frames[4];
extern const uint16_t* const play_frames[4];
extern const uint16_t* const sad_frames[4];

#endif