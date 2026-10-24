#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "game.h"
#ifdef __3DS__
#include <3ds.h>
#include "audio.h"
#endif
#include <stdio.h>
#include <string.h>
const char *lume_scene_names[] = {"GARDEN",    "UPPER DOOR", "FRONT DOOR",
                                  "HALL", "UPSTAIRS",  "POWER ROOM",
                                  "PANTRY"};

const char *lume_scene_assets[] = {"garden",   "roof",  "front", "hall",
                                   "upstairs", "power", "pantry"};

const char *lume_item_names[] = {
    "BLADES",  "KEYS", "WIRE", "SPANNER",
    "TELESCOPE", "DYNAMO",  "POLES"};

const char *lume_action_names[] = {
    "WALK TO HOUSE",      "RETURN TO GARDEN",    "HALL / FRONT DOOR",
    "UPPER DOOR",      "TAKE BLADES",   "FRONT DOOR",
    "SOLAR PANEL",  "LETTER",         "GO UPSTAIRS",
    "DOWN / BACK",  "POWER ROOM", "KEYS",
    "CUPBOARD",         "PIANO / PANTRY", "POWER BOX",
    "METER / POWER",   "MAP",         "GRAMOPHONE",
    "BOOK",         "SPANNER",    "TAKE TELESCOPE",
    "DISMANTLE BIKE", "TAKE DYNAMO",    "TAKE POLES",
    "BUILD TURBINE",   "LOOK AT VILLAGE",       "BOOK 2",
    "BOOK 3",       "PHOTO 1",     "PHOTO 2",
    "PHOTO 3",    "TURN ON POWER",        "LIGHTS",
    "ROOF",          "GARDEN GATE",   "EMERGENCY LIGHT",
    "LOOK AT GRAMOPHONE", "TAKE WIRE"};

static bool has(const LumeGame *g, int item) {
  return (g->inventory & (1u << item)) != 0;
}
static void message(LumeGame *g, const char *s) {
  snprintf(g->message, sizeof(g->message), "%s", s);
}
void lume_game_new(LumeGame *g, uint32_t seed) {
  memset(g, 0, sizeof(*g));
  lume_reset_all(&g->puzzles, seed);
  g->scene = ROOF;
  g->puzzle = -1;
  g->selected = -1;
  g->clue = -1;
  g->book_random = seed;
  g->bike = 1;
  g->turbine = LUME_TURBINE_EMPTY;
  message(g, "SELECT: inventory  X/B: close");
}
int lume_game_actions(const LumeGame *g, int *out, int capacity) {
  if(!g||!out||capacity<=0||(unsigned)g->scene>=LUME_SCENE_COUNT)return 0;
  static const int list[][10] = {
      {GO_HOUSE, BUILD_TURBINE, LOOK_VILLAGE, -1},
      {GO_GARDEN, GO_LADDER, GO_TOP_DOOR, TAKE_BLADES, BUILD_TURBINE,
       LOOK_VILLAGE, -1},
      {GO_LADDER, OPEN_FRONT, OPEN_SOLAR, READ_LETTER, -1},
      {GO_LADDER, GO_STAIRS, GO_POWER, OPEN_KEYS, OPEN_CUPBOARD, OPEN_PANTRY,
       -1},
      {GO_HALL, GO_TOP_DOOR, OPEN_MAP, PLAY_GRAMOPHONE, READ_BOOK, READ_BOOK2,
       READ_BOOK3, LOOK_GRAMOPHONE, -1},
      {GO_HALL, OPEN_FUSE, OPEN_METER, -1},
      {GO_HALL, TAKE_BINOCULARS, DISMANTLE_BIKE, TAKE_DYNAMO, TAKE_POLES, -1}};

  int n = 0;
  for (int i = 0; i < 10 && list[g->scene][i] >= 0; i++)
    if (n < capacity &&
        (list[g->scene][i] != BUILD_TURBINE ||
         (g->scene == GARDEN && g->puzzles.completed[LUME_MAP])))
      out[n++] = list[g->scene][i];

  if (g->scene == HALL && g->clue == 9 && g->puzzles.completed[LUME_CUPBOARD]) {
    unsigned hidden = lume_game_cupboard_mask(g);
    if (!(hidden & 1) && n < capacity) out[n++] = TAKE_TOOLS;
    if (!(hidden & 2) && n < capacity) out[n++] = TAKE_WIRE;
  }
  return n;
}
/* AS1037.updatePower + AS720/frame_1. Derive the stable level from
   completed generators instead of storing a second, potentially stale level. */
unsigned lume_game_power_level(const LumeGame *g) {
  if(!g->fuse || !g->puzzles.completed[LUME_WIRES])return LUME_POWER_ZERO;
  return (unsigned)g->puzzles.completed[LUME_SOLAR]+
         (g->turbine==LUME_TURBINE_CONNECTED);
}
bool lume_game_power_full(const LumeGame *g) {
  return lume_game_power_level(g)==LUME_POWER_FULL;
}
/* AS1037.setUpMouseClick: poles -> dynamo -> blades -> wire. */
int lume_game_turbine_required_item(const LumeGame *g) {
  switch(g->turbine) {
    case LUME_TURBINE_EMPTY:return POLES;
    case LUME_TURBINE_POLES:return DYNAMO;
    case LUME_TURBINE_DYNAMO:return BLADES;
    case LUME_TURBINE_BLADES:return WIRE;
    default:return -1;
  }
}
bool lume_game_action(LumeGame *g, int a) {
  int allowed[12], n = lume_game_actions(g, allowed, 12);
  bool ok = false;
  for (int i = 0; i < n; i++)
    if (allowed[i] == a)
      ok = true;

  if ((a == PRESS_POWER && g->scene == POWER && g->clue == 8) ||
      (a >= SHOW_PHOTO1 && a <= SHOW_PHOTO3 && g->scene == HALL) ||
      (a == LOOK_GATE && g->scene == GARDEN) ||
      (a == LOOK_EMERGENCY_LIGHT && g->scene == POWER) ||
      ((a == LOOK_LIGHTS || a == LOOK_ROOF) &&
       (g->scene == ROOF || g->scene == FRONT)))
    ok = true;
  if (!ok || g->puzzle >= 0 || g->ended)
    return false;

  message(g, "");
  switch (a) {
  case GO_HOUSE:
    g->scene = ROOF;
    break;

  case GO_GARDEN:
    g->scene = GARDEN;
    break;

  case GO_LADDER:
    if (g->scene == HALL)
      g->scene = FRONT;
    else
      g->scene = g->scene == ROOF ? FRONT : ROOF;
    break;

  case GO_TOP_DOOR:
    if (g->scene == UPSTAIRS) {
      g->top_door = true;
      g->scene = ROOF;
    } else if (g->top_door)
      g->scene = UPSTAIRS;
    else
      message(g, "The door is locked from inside.");
    break;

  case TAKE_BLADES:
    if (g->turbine < LUME_TURBINE_BLADES) {
      g->inventory |= 1u << BLADES;
      message(g, "I took the loose fence boards.");
    } else
      message(g, "The boards are on the turbine.");
    break;

  case OPEN_FRONT:
    if (g->puzzles.completed[LUME_DOOR])
      g->scene = HALL;
    else
      g->puzzle = LUME_DOOR;
    break;

  case OPEN_SOLAR:
    if (g->puzzles.completed[LUME_SOLAR])
      message(g, "Solar panel repaired.");
    else
      g->puzzle = LUME_SOLAR;
    break;

  case READ_LETTER:
    g->clue = 0;
    break;

  case GO_STAIRS:
    g->scene = UPSTAIRS;
    break;

  case GO_HALL:
    g->scene = HALL;
    break;

  case GO_POWER:
    g->scene = POWER;
    break;

  case OPEN_KEYS:
    if (has(g, KEYS) || g->fuse)
      message(g, "I found the right keys.");
    else
      g->puzzle = LUME_KEYS;
    break;

  case OPEN_CUPBOARD:
    if (g->puzzles.completed[LUME_CUPBOARD])
      g->clue = 9;
    else
      g->puzzle = LUME_CUPBOARD;
    break;

  case OPEN_PANTRY:
    if (g->puzzles.completed[LUME_PIANO])
      g->scene = PANTRY;
    else
      g->puzzle = LUME_PIANO;
    break;

  case OPEN_FUSE:
    if (!g->fuse) {
      if (g->selected == KEYS && has(g, KEYS)) {
        g->fuse = true;
        /* Source mouseKeys opens the fusebox but does not consume keys. */
        g->selected = -1;
        message(g, "I opened the box with three keys.");
      } else
        message(g, "Select the three keys in the inventory.");
    } else if (g->puzzles.completed[LUME_WIRES])
      message(g, "The wires are connected.");
    else
      g->puzzle = LUME_WIRES;
    break;

  case OPEN_METER:
    g->clue = 8;
    break;
  case PRESS_POWER:
    if (lume_game_power_full(g)) {
      g->ended = true;
      g->clue = -1;
    } else
      message(g, "There is not enough power yet.");
    break;
  case READ_BOOK2:
    g->clue = 3;
    break;
  case READ_BOOK3:
    /* Sprite 674: gotoAndStop(random(11)+2). Choose once per opening,
       never per draw; repeated selections are valid in the original. */
    g->book_random = 1664525u * g->book_random + 1013904223u;
    g->book_variant = (unsigned)(((uint64_t)g->book_random * 11) >> 32);
    g->clue = 4;
    break;
  case SHOW_PHOTO1:
  case SHOW_PHOTO2:
  case SHOW_PHOTO3:
    g->clue = 5 + a - SHOW_PHOTO1;
    break;
  case LOOK_LIGHTS:
    message(g, "The lights need electricity.");
    break;
  case LOOK_ROOF:
    message(g, "Grandpa's house.");
    break;
  case LOOK_GATE:
    message(g, "The garden gate is closed.");
    break;
  case LOOK_EMERGENCY_LIGHT:
    message(g, "Emergency light.");
    break;
  case LOOK_GRAMOPHONE:
    message(g, "I can play the gramophone.");
    break;

  case OPEN_MAP:
    if (g->puzzles.completed[LUME_MAP])
      message(g, "I know where to connect the turbine.");
    else
      g->puzzle = LUME_MAP;
    break;

  case PLAY_GRAMOPHONE:
    g->clue = -1;
    break;

  case READ_BOOK:
    g->clue = 1;
    break;

  case TAKE_TOOLS:
    /* objectSpanner and objectWire have separate AVM1 onRelease handlers.
       Keep the popup open and hide only the object that was actually taken. */
    g->inventory |= 1u << SPANNER;
    message(g, "I took the spanner.");
    break;
  case TAKE_WIRE:
    g->inventory |= 1u << WIRE;
    message(g, "I took the wire.");
    break;

  case TAKE_BINOCULARS:
    g->inventory |= 1u << BINOCULARS;
    message(g, "I took the telescope.");
    break;

  case DISMANTLE_BIKE:
    if (g->selected != SPANNER || !has(g, SPANNER))
      message(g, "Select the spanner in the inventory.");
    else if (g->bike < 3) {
      g->bike++;
      message(g, g->bike == 2 ? "The dynamo is free. I can dismantle the frame too."
                              : "The bike is dismantled. I can take its parts.");
    } else
      message(g, "The bike is already dismantled.");
    break;

  case TAKE_DYNAMO:
    if (g->bike >= 2 && g->turbine < LUME_TURBINE_DYNAMO) {
      g->inventory |= 1u << DYNAMO;
      message(g, "I took the dynamo.");
    } else
      message(g, "Free the dynamo first.");
    break;

  case TAKE_POLES:
    if (g->bike >= 3 && g->turbine < LUME_TURBINE_POLES) {
      g->inventory |= 1u << POLES;
      message(g, "I took the poles.");
    } else
      message(g, "Dismantle the bike first.");
    break;

  case BUILD_TURBINE: {
    int need = lume_game_turbine_required_item(g);

    if (!g->puzzles.completed[LUME_MAP])
      message(g, "Find the location on the map first.");
    else if (g->turbine == LUME_TURBINE_CONNECTED)
      message(g, "The turbine is ready.");
    else if (need >= 0 && g->selected == need && has(g, need)) {
      g->inventory &= ~(1u << need);
      g->turbine++;
      g->selected = -1;
      message(g, g->turbine == LUME_TURBINE_CONNECTED ? "The turbine is connected." : "Part installed.");
    } else
      message(g, g->turbine == LUME_TURBINE_EMPTY   ? "Select the poles."
                 : g->turbine == LUME_TURBINE_POLES ? "Select the dynamo."
                 : g->turbine == LUME_TURBINE_DYNAMO ? "Select the blades."
                                   : "Select the wire.");
    break;
  }
  case LOOK_VILLAGE:
    if (has(g, BINOCULARS) && g->selected == BINOCULARS)
      message(g, "The village has no electricity either.");
    else
      message(g, "A telescope is needed to see the village.");
    break;

  default:
    return false;
  }
  return true;
}
void lume_game_puzzle_result(LumeGame *g) {
  int p = g->puzzle;
  if (p < 0 || p>=LUME_PUZZLE_COUNT || !g->puzzles.completed[p])
    return;
  if (p == LUME_KEYS)
    g->inventory |= 1u << KEYS;
  if (p == LUME_DOOR)
    g->scene = HALL;
  g->puzzle = -1;
  message(g, "Puzzle solved.");
}
#include "source_hit_regions.h"
/* Source geometry was rasterized once, in Flash display-list depth order.
 * Row spans preserve holes and individual book spines without per-touch floats
 * or a large rectangular catch-all that masks neighbouring objects. */
static int source_hit(unsigned region, int x, int y) {
  if (region >= 39 || (unsigned)x >= 320 || y < 13 || y >= 227) return -1;
  unsigned row = region * 241 + (unsigned)y;
  for (unsigned i = source_hit_rows[row]; i < source_hit_rows[row+1]; i++)
    if (x >= source_hit_spans[i].x0 && x < source_hit_spans[i].x1)
      return source_hit_spans[i].action;
  return -1;
}
unsigned lume_game_cupboard_mask(const LumeGame *g) {
  return (has(g, SPANNER) ? 1u : 0u) |
         ((has(g, WIRE) || g->turbine >= LUME_TURBINE_CONNECTED) ? 2u : 0u);
}
unsigned lume_game_pantry_mask(const LumeGame *g) {
  return ((has(g, DYNAMO) || g->turbine >= LUME_TURBINE_DYNAMO) ? 1u : 0u) |
         ((has(g, POLES) || g->turbine >= LUME_TURBINE_POLES) ? 2u : 0u) |
         (has(g, BINOCULARS) ? 4u : 0u);
}
int lume_game_clue_hotspot(const LumeGame *g, int x, int y) {
  if (g->scene == HALL && g->clue == 9 && g->puzzles.completed[LUME_CUPBOARD])
    return source_hit(6 + lume_game_cupboard_mask(g), x, y);
  return -1;
}
int lume_game_hotspot(const LumeGame *g, int x, int y) {
  /* camera3's hotspot_large calls 3_to_2. The source span exporter missed
     this invisible child hit area. Restore only its authored quad:
     x=217.660..320.935, y=12.525..118.950 in touchscreen coordinates.
     Pixel-centre coverage is clipped to the original playable viewport.
     Later source hit areas (such as the roof) keep their depth priority. */
  if (g->scene == FRONT && x >= 218 && x < 320 && y >= 13 && y < 119 &&
      source_hit(FRONT,x,y)<0)
    return GO_LADDER;

  if (g->scene == PANTRY) {
    int stage = g->bike < 1 ? 1 : g->bike > 3 ? 3 : g->bike;
    return source_hit(10 + (stage-1)*8 + lume_game_pantry_mask(g), x, y);
  }
  if (g->scene == GARDEN && g->puzzles.completed[LUME_MAP]) {
    int stage = g->turbine < LUME_TURBINE_EMPTY ? 1 : g->turbine > LUME_TURBINE_CONNECTED ? 5 : g->turbine;
    int action = source_hit(34 + stage - 1, x, y);
    if (action >= 0) return action;
  }
  if (g->scene < GARDEN || g->scene > POWER) return -1;
  return source_hit(g->scene, x, y);
}
/* Fixed-width, versioned snapshot. No raw C struct or pointer serialization. */
static uint32_t crc(const uint32_t *p, int n) {
  uint32_t c = 2166136261u;
  for (int i = 0; i < n; i++)
    for (int b = 0; b < 4; b++) {
      c ^= (p[i] >> (b * 8)) & 255;
      c *= 16777619u;
    }
  return c;
}
void lume_game_snapshot(const LumeGame *g,uint32_t words[18]) {
  uint32_t state[18] = {0x33454d4c,      1,       g->scene,   g->inventory,
                    g->selected + 1, g->bike, g->turbine, g->top_door,
                    g->fuse,         g->ended};
  for (int i = 0; i < 7; i++)
    state[10 + i] = g->puzzles.completed[i];
  state[17] = crc(state, 17);
  memcpy(words,state,sizeof state);
}
static bool load_file(LumeGame *g,const char *path);
bool lume_game_save(const LumeGame *g,const char *path) {
#ifdef __3DS__
  u64 begun=osGetTime(),opened=begun,closed=begun,validated=begun,rotated=begun;
#endif
  uint32_t s[18];lume_game_snapshot(g,s);
  char tmp[320],backup[320];
  if(snprintf(tmp,sizeof tmp,"%s.tmp",path)>=(int)sizeof tmp ||
     snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return false;
  FILE *f=fopen(tmp,"wb");if(!f)return false;
#ifdef __3DS__
  opened=osGetTime();
#endif
  bool ok=fwrite(s,sizeof s,1,f)==1;if(fclose(f))ok=false;
#ifdef __3DS__
  closed=osGetTime();
#endif
  if(!ok){remove(tmp);return false;}
  f=fopen(path,"rb");bool previous=f!=NULL;if(f)fclose(f);
  LumeGame prior;bool rotate=previous&&load_file(&prior,path);
#ifdef __3DS__
  validated=osGetTime();
#endif
  if(rotate){remove(backup);if(rename(path,backup)){remove(tmp);return false;}}
  else if(previous&&remove(path)){remove(tmp);return false;}
#ifdef __3DS__
  rotated=osGetTime();
#endif
  if(rename(tmp,path)){if(rotate)rename(backup,path);remove(tmp);return false;}
#ifdef __3DS__
  lume_log("SAVE_STAGES open=%llu write_close=%llu validate=%llu rotate=%llu publish=%llu",
   (unsigned long long)(opened-begun),(unsigned long long)(closed-opened),
   (unsigned long long)(validated-closed),(unsigned long long)(rotated-validated),
   (unsigned long long)(osGetTime()-rotated));
#endif
  return true;
}
static bool load_file(LumeGame *g, const char *path) {
  uint32_t s[18];
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  bool ok = fread(s, sizeof(s), 1, f) == 1 && fgetc(f) == EOF;
  fclose(f);
  if (!ok || s[0] != 0x33454d4c || s[1] != 1 || s[17] != crc(s, 17) ||
      s[2] >= 7 || s[3] >= 128 || s[4] > 7 || s[5] < 1 || s[5] > 3 ||
      s[6] < 1 || s[6] > 5 || s[7] > 1 || s[8] > 1 || s[9] > 1)
    return false;
  for (int i = 0; i < 7; i++)
    if (s[10 + i] > 1)
      return false;

  lume_game_new(g, 12345);
  g->scene = s[2];
  g->inventory = s[3];
  g->selected = (int)s[4] - 1;
  /* A selected cursor must always refer to an owned inventory item. */
  if (g->selected >= 0 && !has(g, g->selected))g->selected = -1;
  g->bike = s[5];
  g->turbine = s[6];
  g->top_door = s[7];
  g->fuse = s[8];
  g->ended = s[9];
  for (int i = 0; i < 7; i++)
    g->puzzles.completed[i] = s[10 + i];
  message(g, "Save loaded.");
  return true;
}

bool lume_game_load(LumeGame *g,const char *path) {
  if(load_file(g,path))return true;
  char backup[320];if(snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return false;
  return load_file(g,backup);
}
