#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "puzzles.h"
#include <string.h>
#include <stdlib.h>
const int lume_wire_solution[12] = {4, 7, 5, 1, 3, 8, 2, 6, 11, 12, 9, 10};
const int lume_piano_solution[5] = {6, 3, 4, 7, 3};
void wire_reset(WirePuzzle *p) {
  memset(p, 0, sizeof(*p));
  p->selected = -1;
  p->preview_x=p->preview_y=-1;
}
static void disconnect(WirePuzzle *p, int i) {
  int partner = p->links[i];
  p->starts &= ~(1u<<i);
  p->links[i] = 0;
  if (partner > 0 && partner <= 12) {
    p->starts &= ~(1u<<(partner-1));
    p->links[partner - 1] = 0;
  }
}
void wire_touch(WirePuzzle *p, int connector) {
  if (connector < 1 || connector > 12)
    return;
  int i = connector - 1;
  if (p->selected == i) {
    p->selected = -1;
    return;
  }
  if (p->selected < 0) {
    disconnect(p, i);
    p->selected = i;
    return;
  }
  disconnect(p, i);
  p->links[i] = p->selected + 1;
  p->links[p->selected] = i + 1;
  p->starts |= 1u<<p->selected;
  p->selected = -1;
}
bool wire_check(const WirePuzzle *p) {
  return memcmp(p->links, lume_wire_solution, sizeof(p->links)) == 0;
}
void piano_reset(PianoPuzzle *p) { memset(p, 0, sizeof(*p)); }
void piano_note(PianoPuzzle *p, int note) {
  if (note < 1 || note > 8 || p->count >= 5)
    return;
  p->notes[p->count++] = note;
  if (p->count == 5)
    p->solved = memcmp(p->notes, lume_piano_solution, sizeof(p->notes)) == 0;
}
/* AVM1 sprites 553/709, frame 1, setTargets(). */
const int lume_cupboard_codes[3][3] = {{1, 2, 3}, {2, 1, 1}, {8, 8, 7}};
const int lume_door_code[3] = {1, 7, 1};
void solar_reset(SolarPuzzle *p, uint32_t seed) {
  for (int i = 0; i < 64; i++) {
    seed = 1664525u * seed + 1013904223u;
    p->turns[i] = (uint8_t)(seed >> 30);
  }
  if (solar_check(p))
    p->turns[0] = 1;
}
void solar_turn(SolarPuzzle *p, int i) {
  if (i >= 0 && i < 64)
    p->turns[i] = (p->turns[i] + 1) % 4;
}
bool solar_check(const SolarPuzzle *p) {
  for (int i = 0; i < 64; i++)
    if (p->turns[i])
      return false;
  return true;
}
void keys_toggle(KeysPuzzle *p, int i) {
  if (i >= 0 && i < 14)
    p->selected ^= (uint16_t)(1u << i);
}
bool keys_check(const KeysPuzzle *p) { return p->selected == LUME_KEYS_TARGET; }
void lock_reset(LockPuzzle *p) { memset(p, 0, sizeof(*p)); }
void lock_adjust(LockPuzzle *p, int digit, int delta) {
  if (digit < 0 || digit > 2 || p->solved)
    return;
  int n = (p->digits[digit] + delta) % 10;
  if (n < 0)
    n += 10;
  p->digits[digit] = n;
}
bool lock_submit(LockPuzzle *p, bool cupboard) {
  if (p->solved)
    return true;
  const int *target = cupboard ? lume_cupboard_codes[p->stage] : lume_door_code;
  if (memcmp(target, p->digits, sizeof(p->digits)))
    return false;
  p->stage++;
  p->solved = p->stage == (cupboard ? 3 : 1);
  return true;
}
void map_reset(MapPuzzle *p) {
  p->x = p->y = -1;
  p->solved = false;
  p->ink_variant = 0;
}
void map_select(MapPuzzle *p, int x, int y) {
  if (x >= 0 && y >= 0 && x < 10 && y < 10) {
    p->x = x;
    p->y = y;
    p->ink_variant = (uint8_t)(rand() & 3);
  }
}
bool map_submit(MapPuzzle *p) {
  p->solved = p->x == 4 && p->y == 4;
  return p->solved;
}
void lume_reset_one(LumePuzzles *p, int mode, uint32_t seed) {
  if (mode < 0 || mode >= LUME_PUZZLE_COUNT)
    return;
  p->completed[mode] = false;
  switch (mode) {
  case LUME_WIRES:
    wire_reset(&p->wire);
    break;
  case LUME_PIANO:
    piano_reset(&p->piano);
    break;
  case LUME_SOLAR:
    solar_reset(&p->solar, seed);
    break;
  case LUME_KEYS:
    p->keys.selected = 0;
    break;
  case LUME_CUPBOARD:
    lock_reset(&p->cupboard);
    break;
  case LUME_MAP:
    map_reset(&p->map);
    break;
  case LUME_DOOR:
    lock_reset(&p->door);
    break;
  }
}
void lume_reset_all(LumePuzzles *p, uint32_t seed) {
  memset(p, 0, sizeof(*p));
  for (int i = 0; i < LUME_PUZZLE_COUNT; i++)
    lume_reset_one(p, i, seed);
}
bool lume_submit(LumePuzzles *p, int mode) {
  bool accepted = false;
  switch (mode) {
  case LUME_WIRES:
    accepted = wire_check(&p->wire);
    p->completed[mode] = accepted;
    break;
  case LUME_PIANO:
    accepted = p->piano.solved;
    p->completed[mode] = accepted;
    break;
  case LUME_SOLAR:
    accepted = solar_check(&p->solar);
    p->completed[mode] = accepted;
    break;
  case LUME_KEYS:
    accepted = keys_check(&p->keys);
    p->completed[mode] = accepted;
    break;
  case LUME_CUPBOARD:
    accepted = lock_submit(&p->cupboard, true);
    p->completed[mode] = p->cupboard.solved;
    break;
  case LUME_MAP:
    accepted = map_submit(&p->map);
    p->completed[mode] = accepted;
    break;
  case LUME_DOOR:
    accepted = lock_submit(&p->door, false);
    p->completed[mode] = p->door.solved;
    break;
  }
  return accepted;
}
