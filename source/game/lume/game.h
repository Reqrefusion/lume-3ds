#ifndef LUME_GAME_H
#define LUME_GAME_H
#include "puzzles.h"
enum { GARDEN, ROOF, FRONT, HALL, UPSTAIRS, POWER, PANTRY, LUME_SCENE_COUNT };
/* DefineSprite_1037/frame_1: turbineStage stage1..stage5.
   Numeric values are persisted; keep them stable. See docs/ActionScript_Baglantilari.md. */
enum {
  LUME_TURBINE_EMPTY=1,
  LUME_TURBINE_POLES=2,
  LUME_TURBINE_DYNAMO=3,
  LUME_TURBINE_BLADES=4,
  LUME_TURBINE_CONNECTED=5
};
enum { LUME_POWER_ZERO, LUME_POWER_HALF, LUME_POWER_FULL };
enum {
  BLADES,
  KEYS,
  WIRE,
  SPANNER,
  BINOCULARS,
  DYNAMO,
  POLES,
  LUME_ITEM_COUNT
};
enum {
  GO_HOUSE,
  GO_GARDEN,
  GO_LADDER,
  GO_TOP_DOOR,
  TAKE_BLADES,
  OPEN_FRONT,
  OPEN_SOLAR,
  READ_LETTER,
  GO_STAIRS,
  GO_HALL,
  GO_POWER,
  OPEN_KEYS,
  OPEN_CUPBOARD,
  OPEN_PANTRY,
  OPEN_FUSE,
  OPEN_METER,
  OPEN_MAP,
  PLAY_GRAMOPHONE,
  READ_BOOK,
  TAKE_TOOLS,
  TAKE_BINOCULARS,
  DISMANTLE_BIKE,
  TAKE_DYNAMO,
  TAKE_POLES,
  BUILD_TURBINE,
  LOOK_VILLAGE,
  READ_BOOK2,
  READ_BOOK3,
  SHOW_PHOTO1,
  SHOW_PHOTO2,
  SHOW_PHOTO3,
  PRESS_POWER,
  LOOK_LIGHTS,
  LOOK_ROOF,
  LOOK_GATE,
  LOOK_EMERGENCY_LIGHT,
  LOOK_GRAMOPHONE,
  TAKE_WIRE,
  LUME_ACTION_COUNT
};
typedef struct {
  LumePuzzles puzzles;
  int scene, puzzle;
  uint32_t inventory;
  int selected, bike, turbine;
  bool top_door, fuse, ended;
  int clue;
  /* Transient popup state: AVM1 picks a new frame on each book3 opening. */
  uint32_t book_random;
  unsigned book_variant;
  char message[96];
} LumeGame;
extern const char *lume_scene_names[LUME_SCENE_COUNT],
    *lume_scene_assets[LUME_SCENE_COUNT], *lume_item_names[LUME_ITEM_COUNT],
    *lume_action_names[LUME_ACTION_COUNT];
void lume_game_new(LumeGame *g, uint32_t seed);
int lume_game_actions(const LumeGame *g, int *actions, int capacity);
bool lume_game_action(LumeGame *g, int action);
void lume_game_puzzle_result(LumeGame *g);
/* Visible power requires wiring; solar and connected turbine each add one. */
unsigned lume_game_power_level(const LumeGame *g);
int lume_game_turbine_required_item(const LumeGame *g);
bool lume_game_power_full(const LumeGame *g);
int lume_game_hotspot(const LumeGame *g, int x, int y);
/* Popup item hit areas are independent from world-scene hit areas. */
int lume_game_clue_hotspot(const LumeGame *g, int x, int y);
unsigned lume_game_cupboard_mask(const LumeGame *g);
unsigned lume_game_pantry_mask(const LumeGame *g);
bool lume_game_save(const LumeGame *g, const char *path);
bool lume_game_load(LumeGame *g, const char *path);
void lume_game_snapshot(const LumeGame *g,uint32_t words[18]);
#endif
