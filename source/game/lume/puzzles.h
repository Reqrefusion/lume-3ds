#ifndef LUME_PUZZLES_H
#define LUME_PUZZLES_H
#include <stdbool.h>
#include <stdint.h>

/* AVM1 source: sprites 515 (wires), 738 (piano), frame 1.
   Stable source links and archived evidence: docs/actionscript-mapping.json. */
extern const int lume_wire_solution[12];
extern const int lume_piano_solution[5];
typedef struct {
  int links[12];
  int selected;
  uint16_t starts;
  int preview_x, preview_y;
} WirePuzzle;
typedef struct {
  int notes[5];
  int count;
  bool solved;
} PianoPuzzle;
void wire_reset(WirePuzzle *p);
void wire_touch(WirePuzzle *p, int connector);
bool wire_check(const WirePuzzle *p);
void piano_reset(PianoPuzzle *p);
void piano_note(PianoPuzzle *p, int note);
enum {
  LUME_WIRES,
  LUME_PIANO,
  LUME_SOLAR,
  LUME_KEYS,
  LUME_CUPBOARD,
  LUME_MAP,
  LUME_DOOR,
  LUME_PUZZLE_COUNT
};
typedef struct {
  uint8_t turns[64];
} SolarPuzzle;
typedef struct {
  uint16_t selected;
} KeysPuzzle;
typedef struct {
  int digits[3];
  int stage;
  bool solved;
} LockPuzzle;
typedef struct {
  int x, y;
  bool solved;
  uint8_t ink_variant; /* Original circle child picks one of four ink shapes. */
} MapPuzzle;
typedef struct {
  WirePuzzle wire;
  PianoPuzzle piano;
  SolarPuzzle solar;
  KeysPuzzle keys;
  LockPuzzle cupboard, door;
  MapPuzzle map;
  bool completed[LUME_PUZZLE_COUNT];
} LumePuzzles;
extern const int lume_cupboard_codes[3][3];
extern const int lume_door_code[3];
#define LUME_KEYS_TARGET ((uint16_t)((1u << 3) | (1u << 8) | (1u << 10)))
void solar_reset(SolarPuzzle *p, uint32_t seed);
void solar_turn(SolarPuzzle *p, int index);
bool solar_check(const SolarPuzzle *p);
void keys_toggle(KeysPuzzle *p, int index);
bool keys_check(const KeysPuzzle *p);
void lock_reset(LockPuzzle *p);
void lock_adjust(LockPuzzle *p, int digit, int delta);
bool lock_submit(LockPuzzle *p, bool cupboard);
void map_reset(MapPuzzle *p);
void map_select(MapPuzzle *p, int x, int y);
bool map_submit(MapPuzzle *p);
void lume_reset_one(LumePuzzles *p, int mode, uint32_t seed);
void lume_reset_all(LumePuzzles *p, uint32_t seed);
bool lume_submit(LumePuzzles *p, int mode);
#endif
