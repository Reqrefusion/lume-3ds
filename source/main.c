#include "game/lume/close_controls.h"
#include "platform/3ds/video_present.h"
#include "engine/assets/movie_pool.h"
#include "features/settings/stereo_strength.h"
#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "game/lume/original_font.h"
#include "engine/assets/raster.h"
#include "game/lume/actor_pose.h"
#include "adapters/flash/timeline.h"
#include "game/lume/audio.h"
#include "game/lume/persistence.h"
#include "engine/core/log_io.h"
#include "engine/render/blit.h"
#include "game/lume/game.h"
#include "game/lume/clue_views.h"
#include "game/lume/clue_reader.h"
#include "game/lume/document_reading.h"
#include "game/lume/detail_puzzles.h"
#include "engine/render/disparity.h"
#include "engine/render/depth_anchor.h"
#include "engine/assets/stereo_field.h"
#include "engine/assets/depth_stream.h"
#include "engine/render/viewport.h"
#include "features/zoom/hud.h"
#include "game/lume/hover_hints.h"
#include "platform/3ds/frame_flush.h"
#include "game/lume/interaction_flow.h"
#include "game/lume/game_render.h"
#include "engine/codecs/movie.h"
#include "game/lume/skip_policy.h"
#include "platform/3ds/math.h"
#include "game/lume/profile.h"
#include "game/lume/props.h"
#include "game/lume/clip_compose.h"
#include "game/lume/plate_cache.h"
#include "game/lume/puzzles.h"
#include "game/lume/render.h"
#include "game/lume/title_menu.h"
#include "game/lume/logo_depth.h"
#include "game/lume/title_touch.h"
#include "features/reading/bubble.h"
#include "game/lume/native_menu.h"
#include "game/lume/settings_ui.h"
#include "game/lume/title_animation.h"
#include "game/lume/puzzle_flow.h"
#include "game/lume/puzzle_help.h"
#include "game/lume/child_effects.h"
#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
u32 __stacksize__ = 128 * 1024;
static uint8_t backgrounds[7][LUME_FRAME_BYTES], pattern[LUME_PATTERN_BYTES],
    digits[2][30 * 1024];
static uint8_t world[3][LUME_FRAME_BYTES], rendered[3][LUME_FRAME_BYTES],
    stereo[2][LUME_FRAME_BYTES], stereo_pattern[2][LUME_PATTERN_BYTES],
    scratch[LUME_FRAME_BYTES];
static uint8_t solar_tiles[LUME_SOLAR_TILE_BYTES];
static uint8_t map_ink[LUME_MAP_INK_BYTES];
static bool map_ink_ready;
static uint8_t puzzle_depth[LUME_PUZZLE_DEPTH_BYTES];
static bool puzzle_depth_ready;
static bool solar_tiles_ready;
static uint8_t speech[320 * 240 * 4], avatar[19 * 96 * 96 * 4];
static bool avatar_ready, speech_ready, props_ready;
static uint8_t props[320 * 240 * 4];
static bool credits_loaded;
static LumeMovieJobs *clip_jobs;
static bool clip_jobs_checked;
static LumePlateCache plate_cache;
static uint8_t clue_images[3][LUME_FRAME_BYTES];
static int cached_clue = -1;
static LumeClueReader clue_reader={.key=-1};
static LumeViewport viewport;
static LumeViewport viewport_presented;
static bool viewport_presented_valid;
static LumeReading reading;
static LumeNativeMenu native_menu;
static LumeMenuArt native_menu_art;
static bool reading_replace,reading_touch_latch,reading_instruction;
static bool cinema_reading,ending_card_open;
#include "features/reading/timeline.h"
typedef LumeTimedReadingCue LumeEndingCue;
static LumeEndingCue ending_cues[7];
static unsigned ending_cue_count;
static int ending_cue_current=-1;
static bool speech_visual_feedback;
static unsigned manual_save_ticket,ending_save_ticket;
static bool manual_save_quit;
static void ending_cues_load(void) {
 ending_cue_count=0;ending_cue_current=-1;
 FILE *f=fopen("romfs:/ending_reading.tsv","rb");if(!f)return;
 unsigned a,b;char label[64];
 while(ending_cue_count<7&&fscanf(f,"%u%u%63s",&a,&b,label)==3) {
  if(a>b||b>=1908||(ending_cue_count&&a<=ending_cues[ending_cue_count-1].last)||strncmp(label,"ending_",7)){ending_cue_count=0;break;}
  LumeEndingCue *cue=&ending_cues[ending_cue_count++];cue->first=a;cue->last=b;strcpy(cue->label,label);
 }
 fclose(f);
 if(ending_cue_count!=7)ending_cue_count=0;
}
static LumeDocumentReading document_reading={.owner=-1};
static LumeDetail detail,detail_actor,detail_paper,title_menu_detail;
static bool title_touch_active;
static uint8_t title_bottom[LUME_FRAME_BYTES];
static uint8_t *detail_stages;
static bool detail_staged,detail_has_actor;
static char detail_clip_name[64],detail_paper_name[80];
/* Capture the two boundaries around authored foreground layers. Replacing
   their overview pixels after compositing would leave blocky edges and put
   the actor above dialogue. Optional buffers exist only during bottom zoom. */
static void detail_stage(uint8_t *rgb,int stage,bool actor) {
 if(viewport.scale<=1024)return;
 if(!detail_stages)detail_stages=malloc(2*LUME_FRAME_BYTES);
 if(!detail_stages)return;
 memcpy(detail_stages+stage*LUME_FRAME_BYTES,rgb,LUME_FRAME_BYTES);
 if(stage==0)detail_has_actor=actor;
 else detail_staged=true;
}
static void detail_foreground_close(void) {
 lume_detail_close(&detail_actor);lume_detail_close(&detail_paper);
 free(detail_stages);detail_stages=NULL;detail_staged=false;detail_has_actor=false;
}
static int detail_mode=-1;
static const LumePuzzles *detail_state;
static unsigned detail_world_mid=4,detail_world_frame;
static const unsigned detail_initial_frames[]={1602,974,260,377,501,797};
static uint8_t detail_reference[LUME_FRAME_BYTES],detail_world_base[LUME_FRAME_BYTES];
static bool detail_reference_ready;
static bool detail_title_valid,detail_title_exit;
static unsigned detail_title_phase,detail_title_frame,detail_title_exit_frame;
static LumeTitleMenu detail_title_state;


static LumeMoviePool camera_pool={.slots={{.id=-1},{.id=-1}}};
static LumeMovie *movie=&camera_pool.slots[0].clip;
static bool clip_hint_pending;
static const LumeGame *active_game;
static uint8_t movie_base[2][LUME_FRAME_BYTES];
static int cinema_plate = -1;
static unsigned visual_epoch, frame_serial;
static bool rendered_mono;
static bool stereo_enabled=true,stereo_dirty=true;
static LumeStereoField world_field,clue_field,puzzle_field,pattern_field,ending_field;
static LumeDepthAnchor idle_depth_anchor,clip_depth_anchor;
static int idle_actor_quarter_pixels=4;
static LumePresentCache presentation_cache;
static unsigned presentation_dirty;
static LumeAnimation animation;
static int movie_id = -1, clip_kind;
static unsigned clip_last = ~0u;
static unsigned clip_drawn, clip_dropped;
static u64 clip_start, clip_tick_time;
static const char *movie_names[] = {"0175", "0740", "0940", "0945"};
static const char *bubble, *after_clue, *navigation_line;
static int dialogue_target=-1;
static bool speech_modal=true,speech_presented,speech_ack,speech_missing;
static LumeAnimation speech_clip, result_timer, lock_progress, meter_transition;
static uint8_t meter_frame[320*240*3];
static unsigned speech_frame, result_frame=~0u;
static uint64_t speech_started;
static LumePuzzleFlow puzzle_flow={.mode=-1};
static uint8_t solar_completed[LUME_FRAME_BYTES];
static bool solar_completed_ready;
static float actor_x, actor_y;
static LumeInteractionFlow interaction;
static int world_scene = -1;
static const char *navigation_override;
static int status;
static LumeAnimation gramophone_effect, piano_effect;
static LumeAnimation puzzle_art;
static int puzzle_art_mode=-1;
static unsigned puzzle_art_serial;
static LumeChildClock gramophone_clock, piano_clocks[9];
static bool gramophone_pending;
#define feet lume_actor_feet
static bool read_file(const char *path, void *out, size_t bytes) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  bool ok = lume_asset_read(f,out,bytes);
  fclose(f);
  return ok;
}
static void gramophone_tick(uint64_t now,const LumeGame *g) {
  if(g->scene!=UPSTAIRS) {
    if(gramophone_clock.active || gramophone_effect.file)visual_epoch++;
    gramophone_clock.active=false;gramophone_clock.frame=~0u;
    gramophone_pending=false;lume_animation_close(&gramophone_effect);return;
  }
  unsigned old=gramophone_clock.frame;
  unsigned frame=lume_child_tick(&gramophone_clock,now,214);
  if(old!=frame)visual_epoch++;
  unsigned sound;
  while((sound=lume_gramophone_sound(&gramophone_clock,frame)))lume_audio_effect(sound);
  if(!gramophone_clock.active)lume_animation_close(&gramophone_effect);
}
static void draw_gramophone(uint8_t *rgb,int eye) {
  if(gramophone_clock.active && gramophone_clock.frame!=~0u && gramophone_effect.file)
    lume_animation_frame(&gramophone_effect,gramophone_clock.frame,rgb,eye);
}
static void gramophone_actor_frame(unsigned frame,uint64_t now) {
  if(gramophone_pending && frame>=41) {
    gramophone_pending=false;lume_animation_close(&gramophone_effect);
    if(!lume_animation_open(&gramophone_effect,"romfs:/fx_gramophone.lan"))
      lume_log("Gramophone child overlay missing");
    /* Called only once the actor's photographed frame can be composed.
       Cache pauses shift clip_start; the independent child follows it. */
    lume_child_start(&gramophone_clock,clip_start+41*40);
  }
  if(active_game)gramophone_tick(now,active_game);
}
static void piano_press(unsigned key,uint64_t now) {
  if(key>=9)return;
  if(!piano_effect.file && !lume_animation_open(&piano_effect,"romfs:/ui_piano_keys.lan")) {
    lume_log("Piano press overlay missing");return;
  }
  lume_child_start(&piano_clocks[key],now);visual_epoch++;
}
static void piano_tick(uint64_t now,bool visible) {
  for(unsigned key=0;key<9;key++) {
    unsigned old=piano_clocks[key].frame;
    if(!visible)piano_clocks[key].active=false;
    unsigned frame=lume_child_tick(&piano_clocks[key],now,key==8?14:27);
    if(old!=frame)visual_epoch++;
    piano_clocks[key].frame=frame;
  }
  if(!visible)lume_animation_close(&piano_effect);
}
static void draw_piano_presses(uint8_t *rgb) {
  if(!piano_effect.file)return;
  for(unsigned key=0;key<9;key++)if(piano_clocks[key].active && piano_clocks[key].frame!=~0u)
    lume_animation_frame(&piano_effect,(key==8?216:key*27)+piano_clocks[key].frame,rgb,0);
}
static void load_puzzle_art(int mode) {
  if(puzzle_art_mode==mode)return;
  lume_animation_close(&puzzle_art);puzzle_art_mode=mode;
  /* Puzzle-local state must not survive a different board, even if stdio
     reuses the same FILE address for the next art stream. */
  lume_detail_puzzle_cache_close();lume_detail_close(&detail);
  detail_mode=-1;detail_state=NULL;detail_reference_ready=false;
  visual_epoch++;puzzle_art_serial++;
  const char *path=mode==LUME_WIRES?"romfs:/art_wires.lan":mode==LUME_KEYS?"romfs:/art_keys.lan":mode==LUME_DOOR?"romfs:/digits_door.lan":mode==LUME_CUPBOARD?"romfs:/digits_cupboard.lan":NULL;
  if(path && !lume_animation_open(&puzzle_art,path))lume_log("Source puzzle state art missing: %s",path);
}
static void draw_puzzle_art(uint8_t *rgb,const LumePuzzles *p,int mode) {
  if(!puzzle_art.file||puzzle_art_mode!=mode)return;
  if(mode==LUME_DOOR || mode==LUME_CUPBOARD) {
    /* Source differences include the photographed paper which erases the
       previous digit. A dark alpha glyph cannot replace the baked zero. */
    const LockPuzzle *lock=mode==LUME_DOOR?&p->door:&p->cupboard;
    for(unsigned row=0;row<3;row++)
      lume_animation_frame(&puzzle_art,row*10+(unsigned)lock->digits[row],rgb,0);
  } else if(mode==LUME_KEYS) {
    for(unsigned key=0;key<14;key++)if(p->keys.selected&(1u<<key))lume_animation_frame(&puzzle_art,key,rgb,0);
  } else if(mode==LUME_WIRES) {
    /* Authored connector frames distinguish the first and second ends. */
    for(unsigned i=0;i<12;i++) {
      int state=p->wire.selected==(int)i || (p->wire.starts&(1u<<i))?1:p->wire.links[i]?2:0;
      if(state)lume_animation_frame(&puzzle_art,i*2+state-1,rgb,0);
    }
    unsigned pair=24;
    for(unsigned i=0;i<12;i++)for(unsigned j=i+1;j<12;j++,pair++)
      if(p->wire.links[i]==(int)j+1)lume_animation_frame(&puzzle_art,pair,rgb,0);
    lume_render_wire_preview(rgb,&p->wire);
  }
}
static void draw_source_puzzle(uint8_t *rgb,const SceneAssets *view,
    const LumePuzzles *p,int mode,int status,int eye) {
  static uint8_t board[2][LUME_FRAME_BYTES];
  static LumePuzzles previous;
  static const uint8_t *background;
  static const int8_t *previous_depth;
  static int previous_mode=-1,previous_status;
  static FILE *art_file;
  static unsigned art_epoch;
  static bool right_ready;static unsigned previous_strength=~0u;
  const int8_t *depth=puzzle_depth_ready?(const int8_t *)(puzzle_depth+8):NULL;
  if(previous_mode!=mode || previous_status!=status || previous_depth!=depth ||
     background!=view->background[mode] || art_file!=puzzle_art.file || art_epoch!=puzzle_art_serial ||
     memcmp(&previous,p,sizeof previous)) {
    lume_render(board[0],view,p,mode,status);draw_puzzle_art(board[0],p,mode);
    if(mode==LUME_CUPBOARD && lock_progress.file && p->cupboard.stage>0 && p->cupboard.stage<=3)
      lume_animation_frame(&lock_progress,p->cupboard.stage-1,board[0],0);
    memcpy(&previous,p,sizeof previous);previous_mode=mode;previous_status=status;
    background=view->background[mode];art_file=puzzle_art.file;art_epoch=puzzle_art_serial;
    previous_depth=depth;right_ready=false;
  }
  unsigned strength=lume_stereo_strength();
  if(previous_strength!=strength){right_ready=false;previous_strength=strength;}
  if(eye==2&&!right_ready){
    lume_puzzle_reproject_strength(board[0],board[1],depth,strength);right_ready=true;
  }
  /* Speech remains above the stereo board. Touch and bottom zoom continue
     to use the unchanged source/left coordinates. */
  memcpy(rgb,board[eye==2?1:0],LUME_FRAME_BYTES);
}
static void load_clue_images(const LumeGame *g) {
  char path[128];
              int key = lume_clue_cache_key(g);
              if (cached_clue != key) {
                char variant[32];
                const char *name = lume_clue_asset(g,variant,sizeof variant);
                if (!name) name="missing";
                snprintf(path, sizeof(path), "romfs:/clue_%s.rgb", name);
                bool clue_ok=read_file(path, clue_images[0], LUME_FRAME_BYTES);
                if(!clue_ok) {
                  /* Never display a previous clue after an absent/truncated read. */
                  memset(clue_images[0],0,LUME_FRAME_BYTES);
                  lume_draw_text(clue_images[0],75,110,"Image could not be loaded",0xffdc91,1);
                  lume_log("Clue asset missing or invalid: %s",path);
                }
                for (int eye = 1; eye < 3; eye++) {
                  memcpy(clue_images[eye], clue_images[0], LUME_FRAME_BYTES);
                  snprintf(path, sizeof(path), "romfs:/stereo/%s_%c.rgb", name,
                           eye == 2 ? 'R' : 'L');
                  if(clue_ok && !read_file(path, clue_images[eye], LUME_FRAME_BYTES))
                    memcpy(clue_images[eye],clue_images[0],LUME_FRAME_BYTES);
                }
                snprintf(path,sizeof path,"romfs:/clue_%s.rgb",name);
                lume_field_load(&clue_field,path,320,240);
                cached_clue = key;
              }
}
static void overlay(uint8_t *rgb, const uint8_t *rgba, int w, int h, int x,
                    int y) {
  lume_blit_rgba(rgb, rgba, w, h, x, y);
}
/* Input and presentation share the same interpolated camera state so enlarged
   keys, wires and scene hotspots stay aligned. */
static void viewport_touch_read(touchPosition *t) {
  hidTouchRead(t);int x=t->px,y=t->py;
  /* Input arrives before this loop advances/presents the next camera step.
     Hit-test against what the player last saw, including during a zoom. */
  lume_view_touch(viewport_presented_valid?&viewport_presented:&viewport,
                  &x,&y,false);t->px=x;t->py=y;
}
/* The skip gesture belongs to the physical lower screen. Keep it out of
   the first dialogue/world frame until the stylus has been released. */
static bool skip_input(int kind,u32 *down,u32 *held) {
  if(!(*down&KEY_TOUCH)||!lume_skip_visible(kind))return false;
  touchPosition t;hidTouchRead(&t);
  if(!lume_skip_hit(kind,t.px,t.py))return false;
  reading_touch_latch=true;*down=0;*held&=~KEY_TOUCH;return true;
}
static bool viewport_input(u32 down,u32 held,uint64_t now) {
  /* Keep the complete authored camera and actor together during playback. */
  if(animation.file)return lume_view_reset(&viewport,now);
  /* Readable paper is a screen overlay. The independent document camera
     remains usable, especially with paper routed to the upper screen. */
  circlePosition cp;hidCircleRead(&cp);
  /* libctru KEY_UP/RIGHT also include Circle Pad bits. Zoom must use only
     physical D-pad directions; analog displacement belongs solely to pan. */
  int horizontal=!!(held&KEY_DRIGHT)-!!(held&KEY_DLEFT);
  int direction=horizontal?horizontal:!!(held&KEY_DUP)-!!(held&KEY_DDOWN);
  int old_scale=viewport.scale;
  bool pressed=direction && (down&(KEY_DUP|KEY_DDOWN|KEY_DLEFT|KEY_DRIGHT));
  bool changed=horizontal?lume_view_fast_tick(&viewport,now,direction,pressed,cp.dx,cp.dy):
                          lume_view_tick(&viewport,now,direction,pressed,cp.dx,cp.dy);
  if(old_scale==1024&&viewport.scale>1024)visual_epoch++;
  return changed;
}
/* Source pages replace only their clean overview. Actors, inventory and
   feedback retain the original draw order through foreground differences. */
static const uint8_t *detail_title_source(void) {
 if(!detail_title_valid)return NULL;
 int changed=lume_detail_begin(&detail,"title_motion_base",frame_serial);
 if(changed!=1)return changed==0?detail.rgb:NULL;
 bool ok=true;
 if(detail_title_state.has_save)ok=lume_detail_layer(&detail,"title_continue",0)&&ok;
 ok=lume_detail_layer(&detail,detail_title_phase==LUME_TITLE_ENTRY?"title_entry":"title_idle",detail_title_frame)&&ok;
 if(detail_title_state.page==TITLE_CREDITS)ok=lume_detail_layer(&detail,"title_credits",0)&&ok;
 if(detail_title_state.page==TITLE_CONFIRM)ok=lume_detail_layer(&detail,"title_confirm",0)&&ok;
 if(detail_title_state.page==TITLE_MAIN&&detail_title_state.focused) {
  for(int item=detail_title_state.has_save?0:1;item<3;item++) {
   int y0=(int)((item==0?116:item==1?132:147)*2.5f);
   for(int y=y0;y<y0+38;y++)for(int x=350;x<443;x++) {
    uint8_t *c=detail.rgb+(y*800+x)*3;
    if((c[1]>c[0]+40&&c[1]>90&&c[1]>c[2]+10)||(c[0]>90&&c[0]>c[1]+40&&c[1]>c[2]+10)) {
     unsigned v=c[0]>c[1]?c[0]:c[1];
     if(item==detail_title_state.selection){c[0]=v;c[1]=v*95/255;c[2]=0;}
     else {c[0]=0;c[1]=v*205/255;c[2]=v*165/255;}
    }
   }
  }
 }
 if(detail_title_state.page==TITLE_CONFIRM&&detail_title_state.focused) {
  int x0=(int)((detail_title_state.confirm?168:117)*2.5f);
  for(int y=357;y<383;y++)for(int x=x0;x<x0+75;x++)for(int c=0;c<3;c++) {
   uint8_t *v=detail.rgb+(y*800+x)*3+c;*v=*v<225?*v+30:255;
  }
 }
 if(detail_title_exit)ok=lume_detail_layer(&detail,"title_exit",detail_title_exit_frame)&&ok;
 detail.valid=ok;return ok?detail.rgb:NULL;
}
static const uint8_t *detail_source(const uint8_t **reference) {
 const LumeGame *g=active_game;
 if(animation.file)return NULL;
 if(!g){*reference=detail_reference;return detail_title_source();}
 if(g->ended)return NULL;
 if(detail_mode>=0 && detail_state && detail_reference_ready) {
  if(puzzle_flow.mode==LUME_SOLAR && !puzzle_flow.power_preview) {
   if(lume_detail_begin(&detail,"solar_completed",0)>=0){*reference=detail_reference;return detail.rgb;}
   return NULL;
  }
  if(puzzle_flow.power_preview)return NULL;
  unsigned piano[9];for(unsigned i=0;i<9;i++)piano[i]=piano_clocks[i].active?piano_clocks[i].frame:~0u;
  if(lume_detail_puzzle(&detail,detail_mode,detail_state,piano)){
   *reference=detail_reference;return detail.rgb;
  }
  return NULL;
 }
 char name[80],variant[32];
 if(g->clue>=0) {
  const char *asset=lume_clue_asset(g,variant,sizeof variant);
  if(!asset)return NULL;
  snprintf(name,sizeof name,"clue_%s",asset);*reference=clue_images[0];
 } else if(g->scene==PANTRY) {
  snprintf(name,sizeof name,"world_pantry_b%d_m%u",g->bike,lume_game_pantry_mask(g));*reference=world[0];
 } else {
  if(detail_world_mid>=4)return NULL;
  static const unsigned ids[]={175,740,940,945};
  snprintf(name,sizeof name,"world_%04u_%04u",ids[detail_world_mid],detail_world_frame);*reference=detail_world_base;
 }
 if(lume_detail_begin(&detail,name,0)>=0)return detail.rgb;
 return NULL;
}
static bool stereo_poll(void) {
 float value=osGet3DSliderState();
 if(!(value>0.f))value=0.f;
 if(value>1.f)value=1.f;
 unsigned amount=(unsigned)(value*256.f+.5f);
 if(value>0.f&&!amount)amount=1;
 amount=lume_stereo_adjust(amount,lume_locale.stereo_percent);
 if(amount==lume_stereo_strength())return false;
 lume_movie_set_strength(movie,amount);lume_stereo_set_strength(amount);
 stereo_enabled=amount!=0;stereo_dirty=true;visual_epoch++;
 memset(&presentation_cache,0,sizeof presentation_cache);
 lume_log("DISPLAY stereo=%d strength=%u/256 gain=%u%%",stereo_enabled,amount,lume_locale.stereo_percent);return true;
}
static void present(void) {
  if(!clip_jobs_checked) {
    clip_jobs_checked=true;clip_jobs=lume_jobs_create();
    lume_log("DISPLAY_WORKER core=%d",clip_jobs?clip_jobs->core:-1);
  }
  bool read_visible=reading.active&&(!animation.file||cinema_reading)&&!title_touch_active&&
    !(document_reading.active&&native_menu.mode);
  bool read_top=read_visible&&!cinema_reading&&!ending_card_open&&(document_reading.active?document_reading.top:
    active_game&&(reading_instruction||active_game->puzzle>=0||detail_mode>=0||puzzle_flow.mode>=0));
  static int old_read_route=-1;
  int route=read_visible?(read_top?2:1):0;
  if(route!=old_read_route){memset(&presentation_cache,0,sizeof presentation_cache);old_read_route=route;}
  lume_hover_load();
  bool hints=lume_hover_ready&&active_game&&!title_touch_active&&!animation.file&&
    !read_visible&&!speech_ready&&!native_menu.mode&&active_game->puzzle<0&&
    active_game->clue<0&&detail_mode<0&&puzzle_flow.mode<0&&
    (active_game->scene==HALL||active_game->scene==UPSTAIRS);
  static bool old_hints;
  if(hints!=old_hints){memset(&presentation_cache,0,sizeof presentation_cache);old_hints=hints;}
  const uint8_t *bottom=title_touch_active?title_bottom:rendered[0];
  if(!title_touch_active&&!animation.file&&viewport.scale>1024) {
    static uint8_t zoom_canvas[LUME_FRAME_BYTES];
    static unsigned zoom_frame=~0u,zoom_epoch=~0u;
    static int zoom_x,zoom_y,zoom_scale;
    bool zoom_hit=zoom_frame==frame_serial&&zoom_epoch==visual_epoch&&
      zoom_x==viewport.cx&&zoom_y==viewport.cy&&zoom_scale==viewport.scale;
    uint64_t zoom_started=osGetTime();
    if(zoom_hit)memcpy(scratch,zoom_canvas,LUME_FRAME_BYTES);
    else {
    memcpy(scratch,rendered[0],LUME_FRAME_BYTES);
    const uint8_t *reference=rendered[0];
    const uint8_t *source=detail_source(&reference);
    if(source)lume_reader_close(&clue_reader);
    if(!source && active_game && active_game->puzzle<0 && lume_reader_supported(active_game) && lume_reader_load(&clue_reader,active_game)) {
      source=clue_reader.rgb;reference=clue_images[0];
    }
    uint64_t zoom_loaded=osGetTime();
    /* A first detail decode is a preparation cost, not elapsed animation
       time. Prevent that I/O pause from skipping the following zoom steps. */
    if(lume_view_moving(&viewport))viewport.tick+=zoom_loaded-zoom_started;
    lume_view_draw_parallel(&viewport,source?source:rendered[0],source?800:320,source?600:240,scratch,clip_jobs);
    bool staged=active_game&&detail_staged&&detail_reference_ready&&detail_stages;
    int ax=0,ay=0,px=0,py=0;unsigned aw=0,ah=0,pw=0,ph=0;
    const char *pose=staged&&detail_has_actor?lume_actor_detail(active_game->scene):NULL;
    bool actor=pose&&lume_detail_load_layer(&detail_actor,pose,0,&ax,&ay,&aw,&ah);
    char paper_name[80];paper_name[0]=0;
    if(staged&&speech_ready&&!speech_missing&&!(reading.active&&reading_replace)) {
      if(speech_clip.file)snprintf(paper_name,sizeof paper_name,"%s",detail_paper_name);
      else if(bubble)snprintf(paper_name,sizeof paper_name,"speech_%s",bubble);
    }
    bool paper=paper_name[0]&&lume_detail_load_layer(&detail_paper,paper_name,
                     speech_clip.file?speech_frame:0,&px,&py,&pw,&ph);
    if(staged&&(actor||paper)) {
      const uint8_t *before=actor?detail_stages:detail_reference;
      if(source)lume_view_foreground(&viewport,reference,before,scratch);
      else lume_view_draw_parallel(&viewport,before,320,240,scratch,clip_jobs);
      if(actor) {
        ax+=(int)(((int)actor_x-feet[active_game->scene][0])*2.5f);
        ay+=(int)(((int)actor_y-feet[active_game->scene][1])*2.5f);
        lume_detail_overlay_parallel(&detail_actor,&viewport,scratch,ax,ay,aw,ah,clip_jobs);
      }
      if(paper)lume_detail_overlay_parallel(&detail_paper,&viewport,scratch,px,py,pw,ph,clip_jobs);
      else lume_view_foreground(&viewport,detail_reference,detail_stages+LUME_FRAME_BYTES,scratch);
      // Inventory, acknowledgement hints and result covers stay above paper.
      lume_view_foreground(&viewport,detail_stages+LUME_FRAME_BYTES,rendered[0],scratch);
    } else if(source)lume_view_foreground(&viewport,reference,rendered[0],scratch);
    lume_zoom_hud(scratch,(unsigned)viewport.scale);
    memcpy(zoom_canvas,scratch,LUME_FRAME_BYTES);zoom_frame=frame_serial;zoom_epoch=visual_epoch;
    zoom_x=viewport.cx;zoom_y=viewport.cy;zoom_scale=viewport.scale;
    uint64_t zoom_done=osGetTime();static uint64_t zoom_logged;
    if(zoom_done-zoom_started>=80 && zoom_done-zoom_logged>=2000) {
      zoom_logged=zoom_done;
      lume_log("ZOOM_SLOW puzzle=%d scale=%d source_ms=%llu draw_ms=%llu",
       detail_mode,viewport.scale,(unsigned long long)(zoom_loaded-zoom_started),
       (unsigned long long)(zoom_done-zoom_loaded));
    }
    }
    bottom=scratch;
  } /* Keep the bounded page/layer cache when zoom is lowered. Asset keys
       invalidate it on a scene/document change, not on a camera gesture. */
  if(hints){
    if(bottom!=scratch)memcpy(scratch,bottom,LUME_FRAME_BYTES);
    lume_hover_draw(active_game->scene,scratch,320,false,viewport.cx,viewport.cy,viewport.scale);
    bottom=scratch;
  }
  if(read_visible&&!read_top){
    lume_read_draw(&reading,320);
    if(bottom!=scratch)memcpy(scratch,bottom,LUME_FRAME_BYTES);
    lume_read_overlay(&reading,scratch);bottom=scratch;
  }
  bool show_close=active_game&&!active_game->ended&&!animation.file&&!title_touch_active&&!native_menu.mode&&puzzle_flow.mode<0&&
     lume_close_target(active_game,detail_mode>=0,reading_instruction)!=LUME_CLOSE_NONE;
  if(ending_card_open&&!title_touch_active){
    if(bottom!=scratch)memcpy(scratch,bottom,LUME_FRAME_BYTES);
    lume_menu_box(scratch,8,213,304,27,0x24231f);
    lume_ui_center(&native_menu_art,scratch,222,lume_tr("return_menu","Return to Main Menu"),0xf6e8c9,1);
    bottom=scratch;
  }
  if(animation.file&&!cinema_reading&&lume_skip_visible(clip_kind)&&!title_touch_active){
    if(bottom!=scratch)memcpy(scratch,bottom,LUME_FRAME_BYTES);
    lume_skip_draw(scratch);bottom=scratch;
  }
  if(active_game&&!animation.file&&!title_touch_active&&!native_menu.mode&&!read_visible&&
     !speech_ready&&active_game->puzzle<0&&active_game->clue<0&&detail_mode<0){
    if(bottom!=scratch)memcpy(scratch,bottom,LUME_FRAME_BYTES);
    lume_selected_item(active_game,&native_menu_art,scratch);bottom=scratch;
  }
  if(native_menu.mode&&active_game){
    if(bottom!=scratch)memcpy(scratch,bottom,LUME_FRAME_BYTES);
    lume_menu_draw(&native_menu,active_game,&native_menu_art,scratch);
    bottom=scratch;
  }
  /* Physical double buffers already retain a filtered top image. Reuse it
     when only the bottom camera moved; no image comparison or extra copies. */
  /* A translucent top card needs a fresh scene underneath on every present.
     Do not blend again into a cached framebuffer containing an older card. */
  bool gpu_video=(animation.file||title_touch_active)&&!read_top&&!native_menu.mode&&
    lume_gpu_video_present(frame_serial,rendered[0],rendered_mono?rendered[0]:rendered[2],
      gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL),gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,NULL,NULL));
  if(gpu_video) {
    lume_present_bottom(bottom,gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,NULL,NULL));
    presentation_dirty=LUME_TOP_LEFT|LUME_TOP_RIGHT|LUME_BOTTOM;
    memset(&presentation_cache,0,sizeof presentation_cache);
  }else {
    lume_gpu_video_invalidate();
    presentation_dirty=lume_present_views_parallel_cached(read_top?NULL:&presentation_cache,frame_serial,
                   rendered[0],rendered_mono?rendered[0]:rendered[2],bottom,
                   gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL),
                   gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL),
                   gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL),clip_jobs);
  }
  if(show_close){
    lume_close_draw_bottom(gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,NULL,NULL));
    presentation_dirty|=LUME_BOTTOM;
  }
  const LumeGpuVideoStats *gpu=lume_gpu_video_stats();
  static const char *gpu_reported;
  if(gpu->attempted&&gpu_reported!=gpu->state){
    lume_log("GPU_VIDEO state=%s failures=%u",gpu->state,gpu->failures);gpu_reported=gpu->state;
  }
  if(hints){
    uint8_t *l=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL);
    uint8_t *r=gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,NULL,NULL);
    if(presentation_dirty&LUME_TOP_LEFT)lume_hover_draw(active_game->scene,l,400,true,160*1024,120*1024,1024);
    if(r!=l&&(presentation_dirty&LUME_TOP_RIGHT))lume_hover_draw(active_game->scene,r,400,true,160*1024,120*1024,1024);
  }
  if(read_top) {
    lume_read_draw(&reading,400);
    uint8_t *top_left=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL);
    uint8_t *top_right=gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,NULL,NULL);
    lume_read_top(&reading,top_left);
    if(top_right!=top_left)lume_read_top(&reading,top_right);
    presentation_dirty|=LUME_TOP_LEFT|LUME_TOP_RIGHT;
  }
  if(read_visible)reading.presented=true;
  /* GPU DMA completed and invalidated the top buffers. Only CPU-written
     surfaces need another clean before swapping. */
  lume_flush_framebuffers(gpu_video&&!read_top&&!hints?(presentation_dirty&LUME_BOTTOM):presentation_dirty,
    gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,NULL,NULL),
    gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL),
    gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,NULL,NULL));
  viewport_presented=viewport;
  if(title_touch_active||animation.file)lume_view_reset(&viewport_presented,viewport.tick);
  viewport_presented_valid=true;
}
/* Authored title screen and original credits/overwrite dialog. No save file
   is touched until a game has actually been chosen. */
static int show_title_loop(bool has_save) {
  static uint8_t motion_base[LUME_FRAME_BYTES],menu_card[LUME_FRAME_BYTES];
  int menu_card_view=-1;
  if(!lume_title_top_base(&title_menu_detail,motion_base))memset(motion_base,0,sizeof motion_base);
  lume_title_stage_bands(motion_base);
  LumeTitleLayout touch_layout;lume_title_layout(&touch_layout);
  unsigned settings_focus=0;bool settings_error=false;
  bool motion_assets=true;
  LumeTitleMenu state = {0};
  lume_title_init(&state, has_save);
  int last_view = -1, opened_phase=-1, pending=TITLE_NONE;
  unsigned last_frame=~0u,cue_next=0;
  LumeDepthStream title_depth={0};
  LumeAnimation studio_clip={0};
  static uint8_t studio_bottom[LUME_FRAME_BYTES];
  bool studio_ok=lume_animation_open(&studio_clip,"romfs:/studio_intro.lan");
  lume_log("STUDIO_BOTTOM ready=%d frames=%u",studio_ok,studio_clip.frames);
  LumeAnimation title_clip={0},exit_cover={0},title_corner={0},exit_corner={0};
  uint64_t started=osGetTime(),exit_started=0;
  bool motion_ok=motion_assets;
  lume_audio_music_fade_in();
  lume_audio_music(false);
  lume_view_init(&viewport,osGetTime());rendered_mono=true;detail_title_valid=false;
  hidScanInput();
  while (aptMainLoop()) {
    u64 perf_start=svcGetSystemTick();
    hidScanInput();
    u32 down = hidKeysDown();
    bool viewport_dirty=stereo_poll();
    if (down & KEY_START) {
      lume_animation_close(&title_clip);lume_animation_close(&exit_cover);
      lume_animation_close(&title_corner);lume_animation_close(&exit_corner);lume_depth_close(&title_depth);lume_animation_close(&studio_clip);
      return TITLE_QUIT;
    }
    unsigned keys = 0;
    if (down & (KEY_CPAD_UP|KEY_DUP)) keys |= TITLE_UP;
    if (down & (KEY_CPAD_DOWN|KEY_DDOWN)) keys |= TITLE_DOWN;
    if (down & (KEY_CPAD_LEFT|KEY_DLEFT)) keys |= TITLE_LEFT;
    if (down & (KEY_CPAD_RIGHT|KEY_DRIGHT)) keys |= TITLE_RIGHT;
    if (down & KEY_A) keys |= TITLE_OK;
    if (down & KEY_B) keys |= TITLE_BACK;
    if (down & KEY_Y) keys |= TITLE_Y;
    uint64_t input_ticks=(osGetTime()-started)/40;
    bool menu_ready=!motion_ok||lume_title_touch_ready(input_ticks);
    int action=TITLE_NONE;touchPosition touch={0};bool touched=(down&KEY_TOUCH)!=0;
    if(touched)hidTouchRead(&touch);
    if(menu_ready&&pending==TITLE_NONE){
      if(state.page==TITLE_SETTINGS){
        bool changed=lume_settings_input(&settings_focus,keys,touched,touch.px,touch.py);
        if(changed){lume_audio_levels(lume_locale.music,lume_locale.effects);settings_error=!lume_settings_save();}
        if((keys&TITLE_BACK)||(settings_focus==4&&((keys&TITLE_OK)||(touched&&touch.py>=214)))){state.page=TITLE_MAIN;state.focused=true;}
      }else{
        action=lume_title_key(&state,keys);
        if(touched){
          if(state.page==TITLE_MAIN&&touch.px>=24&&touch.px<296){
            for(int i=has_save?0:1;i<4;i++){int y=lume_title_main_y(has_save,i);
              if(touch.py>=y&&touch.py<y+44){state.selection=i;state.focused=true;action=lume_title_activate(&state);break;}}
          }else if(state.page==TITLE_CONFIRM){
            if(touch.py>=120&&touch.py<170){state.confirm=touch.px>=160;action=lume_title_activate(&state);}
          }else if(state.page==TITLE_CREDITS)action=lume_title_touch_bottom(&state,&touch_layout,touch.px,touch.py);
        }
      }
    }
    if (action != TITLE_NONE) {
      if(!motion_ok) { lume_animation_close(&title_clip);lume_animation_close(&exit_cover);
      lume_animation_close(&title_corner);lume_animation_close(&exit_corner);lume_depth_close(&title_depth);lume_animation_close(&studio_clip);return action; }
      pending=action;exit_started=osGetTime();state.page=TITLE_MAIN;
    }
    uint64_t now=osGetTime(), ticks=(now-started)/40;
    unsigned phase=pending!=TITLE_NONE ? LUME_TITLE_EXIT : lume_title_phase(ticks);
    unsigned frame=pending!=TITLE_NONE ? (unsigned)((now-exit_started)/40) : lume_title_frame(ticks);
    if(pending!=TITLE_NONE && frame>=43) {
      // A slow read must not skip the terminal pose before gameplay appears.
      if(last_frame==42) { lume_animation_close(&title_clip);lume_animation_close(&exit_cover);
      lume_animation_close(&title_corner);lume_animation_close(&exit_corner);lume_depth_close(&title_depth);lume_animation_close(&studio_clip);return pending; }
      frame=42;
    }
    uint64_t exit_ticks=pending!=TITLE_NONE ? (exit_started-started)/40 : ticks;
    unsigned under_phase=pending!=TITLE_NONE ? lume_title_phase(exit_ticks) : phase;
    unsigned under_frame=pending!=TITLE_NONE && exit_ticks<736 ? (unsigned)exit_ticks : lume_title_frame(ticks);
    if(motion_ok && (int)under_phase!=opened_phase) {
      const char *paths[]={"romfs:/title_entry.lan","romfs:/title_idle.lan"};
      lume_animation_close(&title_clip);
      motion_ok=lume_animation_open(&title_clip,paths[under_phase]);
      if(!motion_ok)lume_log("Title animation unavailable: %s",paths[under_phase]);
      const char *corners[]={"romfs:/detail/title_touch_entry.lta","romfs:/detail/title_touch_idle.lta"};
      lume_animation_close(&title_corner);lume_animation_open(&title_corner,corners[under_phase]);
      const char *depth_paths[]={"romfs:/title_entry.tsf","romfs:/title_idle.tsf"};
      bool depth_ok=lume_depth_open(&title_depth,depth_paths[under_phase],title_clip.frames);
      lume_log("TITLE_STEREO phase=%u ready=%d frames=%u",under_phase,depth_ok,title_depth.frames);
      opened_phase=(int)under_phase;last_frame=~0u;cue_next=0;
    }
    if(motion_ok && pending!=TITLE_NONE && !exit_cover.file) {
      motion_ok=lume_animation_open(&exit_cover,"romfs:/title_exit.lan");
      lume_animation_open(&exit_corner,"romfs:/detail/title_touch_exit.lta");
      if(!motion_ok)lume_log("Title exit cover unavailable");
    }
    if(!motion_ok && pending!=TITLE_NONE){lume_animation_close(&title_clip);lume_animation_close(&exit_cover);
      lume_animation_close(&title_corner);lume_animation_close(&exit_corner);lume_depth_close(&title_depth);lume_animation_close(&studio_clip);return pending;}
    lume_audio_tick();
    u64 perf_audio=svcGetSystemTick();
    int view=state.page|(state.selection<<2)|(state.confirm<<4)|
      (state.focused<<5)|((int)settings_focus<<6)|(menu_ready<<9)|(int)(lume_locale.serial<<10);
    bool new_frame=view!=last_view || (motion_ok && frame!=last_frame);
    if (new_frame || viewport_dirty) {
      frame_serial++;
      memcpy(rendered[0],motion_base,LUME_FRAME_BYTES);
      if(motion_ok) {
        if(!lume_title_top_layer(&title_clip,&title_corner,under_frame,rendered[0])) {
          motion_ok=false;lume_log("Title frame decode failed: %u/%u",phase,frame);
          memcpy(rendered[0],motion_base,LUME_FRAME_BYTES);
        } else {
          // Dispatch all crossed authored cues once, including simultaneous ones.
          for(;pending==TITLE_NONE && cue_next<=under_frame;cue_next++) {
            if(title_clip.cue_offsets)
              for(unsigned i=title_clip.cue_offsets[cue_next];i<title_clip.cue_offsets[cue_next+1];i++)
                lume_audio_effect(title_clip.cue_ids[i]);
          }
        }
      }
      if(!motion_ok)lume_draw_text(rendered[0],100,45,"LUME",0x00cca5,2);
      unsigned menu_alpha=motion_ok?lume_title_menu_opacity(ticks):255;
      const uint8_t *opening_bottom=rendered[0];
      if(studio_ok&&menu_alpha<255){
        memset(studio_bottom,0,sizeof studio_bottom);
        unsigned studio_frame=lume_title_studio_frame(ticks,studio_clip.frames);
        if(lume_animation_frame(&studio_clip,studio_frame,studio_bottom,0))opening_bottom=studio_bottom;
        else {studio_ok=false;lume_log("STUDIO_BOTTOM decode failed: %u",studio_frame);}
      }else if(studio_clip.file){lume_animation_close(&studio_clip);studio_ok=false;}
      if(!menu_alpha)memcpy(title_bottom,opening_bottom,LUME_FRAME_BYTES);
      else {
        if(menu_card_view!=view) {
          memcpy(menu_card,motion_base,LUME_FRAME_BYTES);
          for(unsigned i=0;i<LUME_FRAME_BYTES;i++)menu_card[i]=menu_card[i]*100/255;
          if(state.page==TITLE_MAIN)lume_title_main_draw(&native_menu_art,menu_card,&state);
          else if(state.page==TITLE_SETTINGS)lume_settings_draw(&native_menu_art,menu_card,settings_focus,settings_error);
          else if(state.page==TITLE_CREDITS)lume_title_bottom_draw(&title_menu_detail,&state,false,menu_card);
          else {
            lume_ui_center(&native_menu_art,menu_card,65,lume_tr("new_confirm","Start a new game?"),0xf6e8c9,2);
            lume_ui_center(&native_menu_art,menu_card,92,lume_tr("overwrite","The existing save will be replaced."),0xf6e8c9,1);
            lume_menu_art_text(&native_menu_art,menu_card,50,132,lume_tr("yes","Yes"),state.confirm==0?0xf79424:0x00cda5,2);
            lume_menu_art_text(&native_menu_art,menu_card,206,132,lume_tr("no","No"),state.confirm==1?0xf79424:0x00cda5,2);
          }
          menu_card_view=view;
        }
        memcpy(title_bottom,menu_card,LUME_FRAME_BYTES);
      }
      if(menu_alpha&&menu_alpha<255)
        lume_title_menu_blend(title_bottom,opening_bottom,LUME_FRAME_BYTES,menu_alpha);
      const int8_t *title_map=stereo_enabled?lume_depth_frame(&title_depth,under_frame):NULL;
      rendered_mono=!stereo_enabled||!title_map;
      if(!rendered_mono){
        lume_puzzle_reproject_strength(rendered[0],rendered[2],title_map,lume_stereo_strength());
        lume_title_stage_bands(rendered[2]);
      }else memcpy(rendered[2],rendered[0],LUME_FRAME_BYTES);
      /* Retain the idle reader/clock beneath animOut: baking the forest into
         the cover would jump it back after an arbitrary wait in the menu. */
      if(motion_ok && pending!=TITLE_NONE &&
         !lume_title_top_layer(&exit_cover,&exit_corner,frame,rendered[0])) {
        motion_ok=false;lume_log("Title exit cover decode failed: %u",frame);
      }
      if(motion_ok&&pending!=TITLE_NONE){
        if(!rendered_mono){
          lume_title_top_layer(&exit_cover,&exit_corner,frame,rendered[2]);
          lume_logo_title_front(rendered[0],rendered[2],lume_stereo_strength());
        }
        lume_title_top_layer(&exit_cover,&exit_corner,frame,title_bottom);
      }
      detail_title_state=state;detail_title_valid=motion_ok;
      detail_title_phase=under_phase;detail_title_frame=under_frame;
      detail_title_exit=pending!=TITLE_NONE;detail_title_exit_frame=frame;
      memcpy(detail_reference,rendered[0],LUME_FRAME_BYTES);

      last_view = view;last_frame=frame;
    }
    u64 perf_draw=svcGetSystemTick();
    if(new_frame || viewport_dirty) {
      present();
      gfxSwapBuffers();
    }
    u64 perf_present=svcGetSystemTick();
    gspWaitForVBlank();
    lume_profile_frame(perf_start,perf_audio,perf_draw,perf_present,svcGetSystemTick(),
                       -2,(int)phase,new_frame||viewport_dirty);
  }
  lume_animation_close(&title_clip);lume_animation_close(&exit_cover);
      lume_animation_close(&title_corner);lume_animation_close(&exit_corner);lume_depth_close(&title_depth);lume_animation_close(&studio_clip);
  return TITLE_QUIT;
}
static int show_title(bool has_save) {
  title_touch_active=true;
  int action=show_title_loop(has_save);
  title_touch_active=false;
  lume_detail_close(&title_menu_detail);
  lume_view_reset(&viewport,osGetTime());
  return action;
}

static void close_clip(void) {
  clip_depth_anchor.valid=false;
  if(cinema_reading){lume_read_close(&reading);cinema_reading=false;}
  gramophone_pending=false; /* Skipping the approach cannot start the record. */
  if (animation.file)
    lume_log("CLIP kind=%d source_fps=%u composed=%u dropped=%u overlay_decodes=%u fast_decodes=%u io_reads=%u plate_hits=%u plate_reads=%u", clip_kind,
             animation.fps,clip_drawn,clip_dropped,animation.decodes,animation.fast_decodes,animation.cache.reads,plate_cache.hits,plate_cache.reads);
  clip_drawn = clip_dropped = 0;
  visual_epoch++;
  cinema_plate = -1;
  lume_animation_close(&animation);
  lume_plate_cache_close(&plate_cache);
  lume_props_close();
  /* Retain the movie index and bounded buffer across camera clips.
     Reopening and reallocating it caused an avoidable pause at each door. */
  lume_movie_sync(movie);
  movie->last = ~0u;
  clip_hint_pending=false;
  clip_kind = 0;
  clip_last = ~0u;
}
static bool camera_open_file(LumeMovie *target,unsigned id) {
 if(id>=4)return false;
 char path[128];const char *extensions[]={"lvs","lvb","ljv"};
 for(unsigned i=0;i<3;i++){
  snprintf(path,sizeof path,"romfs:/media_%s.%s",movie_names[id],extensions[i]);
  if(lume_movie_open(target,path))return true;
 }
 snprintf(path,sizeof path,"sdmc:/3ds/lume_puzzles/media/%s.ljv",movie_names[id]);
 return lume_movie_open(target,path);
}
static bool open_movie(int id) {
 if(id<0||id>=4)return false;
 if(movie_id==id&&movie->file)return true;
 unsigned misses=camera_pool.misses;
 LumeMovie *next=lume_movie_pool_get(&camera_pool,(unsigned)id,camera_open_file);
 if(!next){lume_log("Movie open failed %d",id);return false;}
 movie=next;movie_id=id;
 lume_movie_set_strength(movie,lume_stereo_strength());
 lume_log("CAMERA source=%d reused=%d core=%d",id,camera_pool.misses==misses,movie->stats.worker_core);
 return true;
}
/* Long films already own a bounded resident LAN index. Prepare at most two
   camera ranges before their clock starts. Keep each decoder at a stable
   address, including any cached future-frame jobs that refer to it. */
static void prepare_clip_cameras(int kind) {
 if((kind!=2&&kind!=4)||animation.cache.size!=animation.file_size)return;
 unsigned ids[2],first[2],last[2],count=0;
 for(unsigned f=0;f<animation.frames;f++){
  unsigned mid,bg;if(!lume_animation_camera_at(&animation,f,&mid,&bg)||mid>=4)continue;
  unsigned i=0;for(;i<count;i++)if(ids[i]==mid)break;
  if(i==count){if(count==2)continue;ids[count]=mid;first[count]=last[count]=bg;count++;}
  if(bg<first[i])first[i]=bg;
  if(bg>last[i])last[i]=bg;
 }
 for(unsigned i=0;i<count;i++){
  u64 started=osGetTime();
  if(!open_movie((int)ids[i])||last[i]>=movie->frames)continue;
  unsigned bytes=movie->offsets[last[i]+1]-movie->offsets[first[i]];
  bool resident=bytes<=6u*1024u*1024u&&lume_movie_prime(movie,first[i],last[i]);
  bool decoded=resident&&lume_movie_frame(movie,first[i],movie_base[0],stereo_enabled?movie_base[1]:NULL);
  lume_log("CAMERA_WARM source=%u first=%u last=%u resident=%d decoded=%d bytes=%u ms=%llu",
   ids[i],first[i],last[i],resident,decoded,bytes,(unsigned long long)(osGetTime()-started));
 }
}
static bool play_clip(const char *name, int kind) {
  detail_foreground_close();
  snprintf(detail_clip_name,sizeof detail_clip_name,"%s",name);
  close_clip();
  char path[128];
  snprintf(path, sizeof(path), "romfs:/%s.lan", name);
  if (!lume_animation_open(&animation, path)) {
    lume_log("Animation missing: %s", name);
    return false;
  }
  /* Long, mixed-camera films cannot prefetch one video range. Keep their
     bounded LAN bytes and recurring plates resident before starting the
     authored clock; otherwise a rolling 1 MiB refill blocks visible frames. */
  if(kind==2 || kind==4) {
    u64 warm_start=osGetTime();
    bool resident=animation.file_size<=6u*1024u*1024u &&
      lume_cache_prime(&animation.cache,0,animation.file_size);
    unsigned plates[LUME_PLATE_SLOTS],count=0;
    if(resident)for(unsigned f=0;f<animation.frames;f++) {
      unsigned mid,bg;
      if(!lume_animation_camera_at(&animation,f,&mid,&bg) || mid!=4)continue;
      bool found=false;for(unsigned j=0;j<count;j++)if(plates[j]==bg)found=true;
      if(found)continue;
      if(count==LUME_PLATE_SLOTS)break;
      plates[count++]=bg;
      if(!lume_plate_cache_frame(&plate_cache,bg,stereo_enabled,
                                  movie_base[0],movie_base[1],read_file))break;
    }
    lume_log("CLIP_WARM asset=%s resident=%d bytes=%lu plates=%u ms=%llu",
      name,resident,(unsigned long)animation.file_size,count,
      (unsigned long long)(osGetTime()-warm_start));
  }
  if (kind == 1 && !lume_props_open(name, animation.frames))
    lume_log("Prop transforms missing: %s", name);
  unsigned mid, first, last;
  uint32_t meta[5] = {0};
  snprintf(path, sizeof(path), "romfs:/%s.nav", name);
  bool known = read_file(path, meta, sizeof(meta)) &&
               !memcmp(meta, "NAV2", 4) && meta[1] < 4 && meta[2] <= meta[3];
  if (known) {
    mid = meta[1];
    first = meta[2];
    last = meta[3];
  } else {
    meta[4] = 0;
    known = lume_animation_camera_range(&animation, &mid, &first, &last);
  }
  /* A photographed dissolve may be larger than an ordinary actor crop.
     Never shrink the read window below the largest checked LAN packet. */
  if (known && meta[4] >= animation.packet_capacity && meta[4] >= 65536 &&
      meta[4] < animation.cache.capacity)
    animation.cache.capacity = meta[4];
  if (known && open_movie(mid) && last < movie->frames) {
    bool async =
        lume_movie_prefetch(movie,first,last);
    lume_log("PREFETCH kind=%d async=%d bytes=%lu", kind, async,
             (unsigned long)(movie->offsets[last + 1] - movie->offsets[first]));
  }
  prepare_clip_cameras(kind);
  lume_animation_frame(&animation, 0, NULL, 0);
  lume_log("Animation %s: %u frames, SFX2=%u, legacy=%d", name,
           animation.frames, animation.cue_count, animation.cues != NULL);
  clip_kind = kind;
  if(kind==4)ending_cues_load();
  /* A clip may open after this loop's input; center the camera immediately. */
  lume_view_reset(&viewport,osGetTime());
  clip_start = clip_tick_time = osGetTime();
  clip_last = ~0u;
  return true;
}
static void load_world(const LumeGame *g) {
  idle_depth_anchor.valid=false;
  detail_world_mid=g->scene<6?0:4;
  if(g->scene<6)detail_world_frame=detail_initial_frames[g->scene];
  visual_epoch++;
  if (world_scene != g->scene) {
    lume_interaction_reset(&interaction,g->scene);
    world_scene = g->scene;
    lume_actor_restore(g->scene);
  }
  char path[128], name[32];
  snprintf(name, sizeof(name), "%s", lume_scene_assets[g->scene]);
  if (g->scene == PANTRY)
    snprintf(name, sizeof(name), "pantry_b%d_m%u", g->bike,
             lume_game_pantry_mask(g));
  if ((g->scene == ROOF || g->scene == GARDEN) && g->turbine > LUME_TURBINE_EMPTY)
    snprintf(name, sizeof(name), "%s%d", lume_scene_assets[g->scene],
             g->turbine);
  /* Inventory removal changes the fence immediately, independently of the
     turbine assembly stage or the next camera transition. */
  if((g->scene==ROOF || g->scene==GARDEN) && g->turbine<LUME_TURBINE_BLADES &&
     (g->inventory & (1u<<BLADES)))
    strncat(name,"_bare",sizeof(name)-strlen(name)-1);
  snprintf(path, sizeof(path), "romfs:/world_%s.rgb", name);
  if (!read_file(path, world[0], LUME_FRAME_BYTES))
    memset(world[0], 0, LUME_FRAME_BYTES);
  lume_field_load(&world_field,path,320,240);
  memcpy(detail_world_base,world[0],LUME_FRAME_BYTES);
  if(g->scene==GARDEN||g->scene==ROOF) {
    /* Variant plates can contain newly assembled objects. Compare against
       the clean photographic plate so those pixels survive native zoom. */
    snprintf(path,sizeof path,"romfs:/world_clean_%s.rgb",lume_scene_assets[g->scene]);
    if(!read_file(path,detail_world_base,LUME_FRAME_BYTES))detail_world_mid=4;
  }
  for (int i = 2; i < 3; i++) {
    snprintf(path, sizeof(path), "romfs:/stereo/%s_%c.rgb", name,
             i == 1 ? 'L' : 'R');
    if (!read_file(path, world[i], LUME_FRAME_BYTES))
      memcpy(world[i], world[0], LUME_FRAME_BYTES);
  }
  /* Recompose only when source visibility differs from the baked plate.
     Reuse resident prop images; work happens on state/scene changes only. */
  if((g->scene==GARDEN||g->scene==ROOF)&&detail_world_mid!=4&&
     (!g->puzzles.completed[LUME_MAP]||g->puzzles.completed[LUME_SOLAR])) {
    memcpy(scratch,detail_world_base,LUME_FRAME_BYTES);
    bool blades=!(g->inventory&(1u<<BLADES))&&g->turbine<LUME_TURBINE_BLADES;
    if(lume_props_world(scratch,(unsigned)g->scene,g->turbine,blades,
         g->puzzles.completed[LUME_MAP],!g->puzzles.completed[LUME_SOLAR],0)) {
      memcpy(world[0],scratch,LUME_FRAME_BYTES);
      if(world_field.valid)lume_field_dynamic(&world_field,world[0],world[2],256);
      else memcpy(world[2],world[0],LUME_FRAME_BYTES);
    }
  }
  snprintf(path, sizeof(path), "romfs:/props_%s_%d.rgba",
           lume_scene_assets[g->scene],
           (g->inventory & (1u << BLADES)) || g->turbine >= LUME_TURBINE_BLADES ? 0 : 1);
  props_ready = read_file(path, props, sizeof(props));
  actor_x = feet[g->scene][0];
  actor_y = feet[g->scene][1];
}
static void say(const char *label) {
  lume_document_close(&document_reading,&reading);
  reading_instruction=false;
  visual_epoch++;
  lume_animation_close(&speech_clip);
  bubble = label;detail_paper_name[0]=0;speech_visual_feedback=false;
  reading_replace=lume_read_open(&reading,label);
  if(label&&!reading_replace)lume_log("READING_FALLBACK label=%s",label);
  speech_modal=true;speech_presented=false;speech_ack=false;speech_missing=false;speech_frame=0;
  if(!label)dialogue_target=-1;
  speech_ready = reading_replace;
  if (label&&!reading_replace) {
    char path[128];
    snprintf(path, sizeof(path), "romfs:/speech_%s.rgba", label);
    speech_ready = read_file(path, speech, sizeof(speech));
    if(!speech_ready) {
      memset(speech,0,sizeof speech);speech_missing=true;speech_ready=true;
      lume_log("Dialogue image missing: %s",path);
    }
  }
}
/* A popup has its own source dialogue matrix. Reusing one universal paper
   image for books, keys and the map loses their original placement. */
static bool clip_has_art(LumeAnimation *clip) {
  for(unsigned i=0;i<clip->frames;i++) {
    const uint8_t *p=lume_cache_get(&clip->cache,clip->offsets[i],12);
    if(p && (p[8]||p[9]) && (p[10]||p[11]))return true;
  }
  return false;
}
static void popup_say(const char *asset,const char *fallback) {
  say(fallback);
  /* These popups also contain baked low-resolution text. Keep the
     authored feedback clock, but route available text through the reader.
     Solar dialogue uses only the reader, including its blank paper.
     The power fade remains an effect rather than a dialogue card. */
  if(!strcmp(asset,"power_fade")) {
    lume_read_close(&reading);reading_replace=false;
  }
  speech_visual_feedback=!strcmp(asset,"wires_correct")||!strcmp(asset,"wires_incorrect")||
    !strcmp(asset,"map_correct")||!strcmp(asset,"power_fade");
  if(reading_replace&&!speech_visual_feedback)return;
  if(fallback)lume_log("DIALOGUE_LEGACY asset=%s label=%s",asset,fallback);
  char path[128];snprintf(path,sizeof path,"romfs:/ui_%s.lan",asset);
  if(lume_animation_open(&speech_clip,path) && clip_has_art(&speech_clip)) {
    speech_started=osGetTime();speech_frame=0;speech_ready=true;
    snprintf(detail_paper_name,sizeof detail_paper_name,"ui_%s",asset);
  } else {
    lume_animation_close(&speech_clip);
    lume_log("Popup feedback missing/empty; use paper fallback: %s",path);
  }
}
static void speech_tick(uint64_t now) {
  if(speech_clip.file) {
    // A movement completion can open this paper after the loop captured now.
    // Clamp that older timestamp instead of underflowing into the last frame.
    uint64_t tick=now<speech_started?0:(now-speech_started)/40;
    unsigned f=tick>=speech_clip.frames?speech_clip.frames-1:(unsigned)tick;
    if(f!=speech_frame){speech_frame=f;visual_epoch++;}
  }
}
static void draw_speech(uint8_t *rgb) {
  if(reading.active&&reading_replace&&!speech_visual_feedback){speech_ack=true;return;}
  if(speech_clip.file) {
    if(lume_animation_frame(&speech_clip,speech_frame,rgb,0)) {
      // Several source entrances begin with transparent frames. Seeing such a
      // frame is not acknowledgement: the paper must finish entering first.
      speech_ack=speech_frame==speech_clip.frames-1 && speech_clip.width && speech_clip.height;
      return;
    }
    lume_log("Dialogue frame decode failed; retain readable fallback");
    lume_animation_close(&speech_clip);
  }
  if(reading.active&&reading_replace){speech_ack=true;return;}
  if(speech_ready) {
    if(speech_missing)lume_draw_text(rgb,150,100,"Dialogue could not be loaded",0xffdc91,1);
    else overlay(rgb,speech,320,240,0,0);
    speech_ack=true;
  }
}
static void close_clue(LumeGame *g) {
  const char *line=after_clue;after_clue=NULL;g->clue=-1;
  say(line);
}
static void puzzle_entry_speech(int mode) {
  if(mode==LUME_KEYS)popup_say("keys_seen","keyPuzzleSeen");
  else if(mode==LUME_MAP)popup_say("map_seen","mapPuzzleSeen");
}
static void dismiss_speech(LumeGame *g,bool accept) {
  int target=dialogue_target;
  say(NULL);
  if(accept && target>=0) {
    g->puzzle=target;status=0;
    puzzle_entry_speech(target);
  }
}
/* Instruction cards use the readable paper without opening authored dialogue.
   Closing a card must not accept a pending conversation or complete a puzzle. */
static void puzzle_help_layout(void) {
  lume_read_layout(&reading,400);
  reading.pages=0;reading.page=0;unsigned at=0;
  while(reading.text[at]&&reading.pages<LUME_READING_PAGES) {
    reading.offsets[reading.pages++]=at;
    unsigned end=at,split=0;
    for(unsigned i=0;i<LUME_READING_LINES&&reading.text[end];i++)
      end=lume_read_line(reading.text,end,lume_read_columns(&reading,400),NULL);
    if(reading.text[end]&&reading.text[end]!='-')for(unsigned i=at+1;i<end;i++) {
      if(reading.text[i]!='-'||reading.text[i+1]!=' ')continue;
      unsigned ch=(unsigned char)reading.text[i+2];
      /* The map's "East 5 - 3" is arithmetic inside an instruction.
         Only a source dash followed by a word can start the next step. */
      if(reading.text[i]=='-'&&reading.text[i+1]==' '&&
         ((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')))split=i;
    }
    at=split?split:end;
  }
  reading.serial++;
}
static bool puzzle_help_open(int mode,int x,int y,u32 *down,u32 *held) {
  if(animation.file||native_menu.mode||reading.active||bubble||puzzle_flow.mode>=0)return false;
  const char *label=lume_puzzle_help_label(mode,x,y);
  if(!label)return false;
  if(!lume_read_open(&reading,label)) {
    lume_log("Instruction text missing: %s",label);return false;
  }
  puzzle_help_layout();
  reading_instruction=true;reading_replace=false;visual_epoch++;
  reading_touch_latch=true;*down=0;*held&=~KEY_TOUCH;
  return true;
}
/* Reading consumes its own input before any puzzle reset/test/hotspot path. */
/* The release latch covers held-touch puzzle drags as well as press edges. */
static void reading_block_touch(u32 *down,u32 *held) {
  if(!reading_touch_latch)return;
  if(!(*held&KEY_TOUCH))reading_touch_latch=false;
  *down&=~KEY_TOUCH;*held&=~KEY_TOUCH;
}
/* Document paper has its own Y/L/R controls. B and the existing document
   close touch stay with the photographed page; neither dismisses dialogue. */
static void document_input(LumeGame *g,u32 *down,u32 *held) {
 if(lume_document_sync(&document_reading,&reading,g))visual_epoch++;
 if(animation.file||native_menu.mode||bubble||reading_instruction)return;
 if(document_reading.active){
  if(*down&KEY_B)return;
  bool changed=false;
  if(*down&KEY_R)changed|=lume_document_route(&document_reading,&reading,true);
  else if(*down&KEY_L)changed|=lume_document_route(&document_reading,&reading,false);
  if(*down&KEY_Y)changed|=lume_document_next(&document_reading,&reading);
  if(changed)visual_epoch++;
  *down&=~(KEY_Y|KEY_L|KEY_R);*held&=~(KEY_Y|KEY_L|KEY_R);
 }else if((*down&KEY_Y)&&!reading.active){
  if(lume_document_open(&document_reading,&reading,g)){
   reading_replace=false;visual_epoch++;
   *down&=~(KEY_Y|KEY_L|KEY_R);*held&=~(KEY_Y|KEY_L|KEY_R);
  }
 }
}
static void reading_input(LumeGame *g,u32 *down,u32 *held) {
if(!animation.file&&!ending_card_open&&reading.active&&!document_reading.active) {
      bool back=((*down)&KEY_B)!=0;
      bool next=((*down)&(KEY_A|KEY_TOUCH))!=0;
      if((*down)&KEY_TOUCH)reading_touch_latch=true;
      if(back||next) {
        if(lume_read_step(&reading,back))visual_epoch++;
        if(reading.finished&&reading_instruction) {
          lume_read_close(&reading);reading_instruction=false;visual_epoch++;
        } else if(reading.finished&&puzzle_flow.mode<0)dismiss_speech(g,!reading.cancel);
      }
      // The press that changes a page or opens a puzzle is consumed once.
      // The lower touch surface advances paper on either screen.
      // Keep this gesture out of the board until the finger is released.
      *down=0;*held&=~KEY_TOUCH;
    }
}
static bool reading_result_ready(uint64_t now) {
 return lume_puzzle_flow_ready(&puzzle_flow,now)&&(!reading.active||reading.finished);
}
typedef struct {
  int size;
  uint8_t map[128];
  float offset_y;
} AvatarScale;
static AvatarScale avatar_scales[LUME_SCENE_COUNT];
static void avatar_scales_init(void) {
  for (int scene = 0; scene < LUME_SCENE_COUNT; scene++) {
    float height = lume_actor_heights[scene];
    float scale = height / 78.f;
    AvatarScale *a = &avatar_scales[scene];
    a->size = (int)(96 * scale);
    a->offset_y = 92 * scale;
    /* Cold table build retains the original sampling exactly; no VFP crossings
     * per pixel. */
    for (int i = 0; i < a->size; i++)
      a->map[i] = (int)(i / scale);
  }
}
/* @hotpath Integer lookup sampling; float-to-int conversion happens only before
 * the pixel loops. */
static void draw_avatar(uint8_t *rgb, int eye, const LumeGame *g) {
  if (g->scene == PANTRY)
    return;
  if(!eye) {
    const int8_t *map=world_field.valid?world_field.map:NULL;
    int foot_x=(int)actor_x,foot_y=(int)actor_y;
    (void)lume_actor_ground(g->scene,(int)actor_x,(int)actor_y,&foot_x,&foot_y);
    int target=lume_depth_ground(map,foot_x,foot_y);
    idle_actor_quarter_pixels=lume_depth_follow(&idle_depth_anchor,target,
      foot_x,foot_y,(unsigned)g->scene,detail_world_frame,frame_serial);
  }
  if (lume_actor_idle(rgb, g->scene, (int)actor_x, (int)actor_y, eye,idle_actor_quarter_pixels))
    return;
  if (!avatar_ready)
    return;
  unsigned frame = 18; /* Idle-only fallback; walking always comes from a source clip. */
  const uint8_t *base = avatar + frame * 96 * 96 * 4;
  const AvatarScale *a = &avatar_scales[g->scene];
  int w = a->size;
  int origin_x=(int)(actor_x-w/2);
  int y = (int)(actor_y - a->offset_y);
  unsigned strength=lume_stereo_strength();
  int offset=eye==2?idle_actor_quarter_pixels*(int)strength/4:0;
  int x=origin_x-offset/256;
  if(offset%256 && w<=128) {
    uint8_t scan[128*4];
    for(int yy=0;yy<w;yy++) {
      const uint8_t *source=base+a->map[yy]*96*4;
      for(int xx=0;xx<w;xx++)memcpy(scan+xx*4,source+a->map[xx]*4,4);
      lume_blit_rgba_parallax(rgb,scan,w,1,origin_x,y+yy,eye,idle_actor_quarter_pixels);
    }
    return;
  }
  for (int yy = 0; yy < w; yy++) {
    int dy = y + yy;
    if ((unsigned)dy >= 240)
      continue;
    const uint8_t *row = base + a->map[yy] * 96 * 4;
    for (int xx = 0; xx < w; xx++) {
      int dx = x + xx;
      if ((unsigned)dx >= 320)
        continue;
      unsigned sx = a->map[xx];
      const uint8_t *p = row + sx * 4;
      unsigned alpha = p[3];
      if (!alpha)
        continue;
      uint8_t *d = rgb + (dy * 320 + dx) * 3;
      if (alpha == 255) {
        d[0] = p[0];
        d[1] = p[1];
        d[2] = p[2];
        continue;
      }
      lume_blend_pixel(d,p,alpha);
    }
  }
}
static bool scene_needs_redraw(const LumeGame *g,
                               const LumePuzzles *debug_state, bool debug,
                               int mode, bool zoom, int zx, int zy) {
  static LumeGame old_game;
  static LumePuzzles old_debug;
  static int old_stamp[12];
  static unsigned epoch = ~0u;
  int stamp[12] = {mode,
                   0,
                   status,
                   zoom,
                   zx,
                   zy,
                   debug,
                   (int)actor_x,
                   (int)actor_y,
                   18,
                   interaction.position,
                   0};
  if (epoch == visual_epoch && !memcmp(&old_game, g, sizeof(*g)) &&
      !memcmp(&old_debug, debug_state, sizeof(*debug_state)) &&
      !memcmp(old_stamp, stamp, sizeof(stamp)))
    return false;
  memcpy(&old_game, g, sizeof(*g));
  memcpy(&old_debug, debug_state, sizeof(*debug_state));
  memcpy(old_stamp, stamp, sizeof(stamp));
  epoch = visual_epoch;
  return true;
}
static void load_puzzle_stereo(int p) {
  char path[128];
  snprintf(path,sizeof path,"romfs:/stereo/%s.depth",lume_asset_names[p]);
  puzzle_depth_ready=read_file(path,puzzle_depth,sizeof puzzle_depth) &&
    !memcmp(puzzle_depth,"PDP1",4) && puzzle_depth[4]==64 && puzzle_depth[5]==1 &&
    puzzle_depth[6]==240 && puzzle_depth[7]==0;
  if(!puzzle_depth_ready && (p==LUME_WIRES||p==LUME_KEYS||p==LUME_DOOR||p==LUME_CUPBOARD))
    lume_log("Puzzle depth missing; regenerate EXE assets: %s",path);
  snprintf(path,sizeof path,"romfs:/stereo/%s_L.rgb",lume_asset_names[p]);
  lume_field_load(&puzzle_field,path,320,240);
  lume_field_load(&pattern_field,"romfs:/stereo/solar_pattern_L.rgb",160,160);
  for (int i = 0; i < 2; i++) {
    snprintf(path, sizeof(path), "sdmc:/3ds/lume_puzzles/stereo/%s_%c.rgb",
             lume_asset_names[p], i ? 'R' : 'L');
    if (!read_file(path, stereo[i], LUME_FRAME_BYTES)) {
      snprintf(path, sizeof(path), "romfs:/stereo/%s_%c.rgb",
               lume_asset_names[p], i ? 'R' : 'L');
      if (!read_file(path, stereo[i], LUME_FRAME_BYTES))
        memcpy(stereo[i], backgrounds[p], LUME_FRAME_BYTES);
    }
    snprintf(path, sizeof(path), "romfs:/stereo/solar_pattern_%c.rgb",
             i ? 'R' : 'L');
    if (!read_file(path, stereo_pattern[i], LUME_PATTERN_BYTES))
      memcpy(stereo_pattern[i], pattern, LUME_PATTERN_BYTES);
  }
}


static void dispatch(LumeGame *g, int action) {
  u64 action_begin = svcGetSystemTick();
  int old = g->scene;
  int old_bike = g->bike, old_turbine = g->turbine;
  uint32_t old_inventory = g->inventory;
  bool was_ended = g->ended;
  bool was_top_door=g->top_door,was_fuse=g->fuse;
  if (!lume_game_action(g, action))
    return;
  lume_log("Action %d scene %d -> %d", action, old, g->scene);
  status = 0;
  const char *line = NULL;
  switch (action) {
  case GO_TOP_DOOR:
    if (g->scene == old)line="lockedGeneric";
    else if(old==UPSTAIRS && !was_top_door)navigation_line="unlockedGeneric";
    break;
  case OPEN_SOLAR:
    line=g->puzzles.completed[LUME_SOLAR]?"rotationPuzzleComplete":"rotationPuzzleIntro";
    break;
  case OPEN_MAP:
    line=g->puzzles.completed[LUME_MAP]?"puzzleComplete":"deskLook";
    break;
  case OPEN_KEYS:
    if(g->puzzle<0)line="puzzleComplete";
    break;
  case OPEN_PANTRY:
    if(g->puzzle==LUME_PIANO)line="broomcupboardLocked";
    break;
  case TAKE_BLADES:
    /* Sprite 806/frame 89 speaks after taking the fence blades.
       Bind feedback to the committed inventory change, not a later idle frame. */
    line = (old_inventory&(1u<<BLADES)) || old_turbine>=LUME_TURBINE_BLADES ? "puzzleComplete" : "bladesTaken";
    break;
  case LOOK_LIGHTS:
    line = "fairylights";
    break;
  case LOOK_ROOF:
    line = "roof";
    break;
  case LOOK_GATE:
    line = "gateClosed";
    break;
  case LOOK_EMERGENCY_LIGHT:
    line = "emergencyLight";
    break;
  case OPEN_FUSE:
    if(!g->fuse)line="fuseboxLocked";
    else if(!was_fuse)line="fuseboxUnlocked";
    else if(g->puzzles.completed[LUME_WIRES])line="puzzleComplete";
    break;
  case LOOK_VILLAGE:
    line = (g->inventory&(1u<<BINOCULARS))?"useBinoculars":"villageFarAway";
    break;
  case PRESS_POWER:
    if (!g->ended)
      line = "meterShouldBeFull";
    break;
  default:
    break;
  }
  if (!line && g->clue < 0 && g->puzzle < 0) {
    switch (action) {
    case TAKE_TOOLS:
      line = "cupboardPuzzleComplete";
      break;
    case TAKE_BINOCULARS:
      line = "binocularsTaken";
      break;
    case TAKE_DYNAMO:
      line = g->bike >= 2 ? "dynamoFree" : "dynamoFixed";
      break;
    case TAKE_POLES:
      line = g->bike >= 3 ? "polesFree" : "bikeThought";
      break;
    case DISMANTLE_BIKE:
      line = g->bike == 1   ? "bikeThought"
             : g->bike == 2 ? "dynamoFree"
                            : "polesFree";
      break;
    case BUILD_TURBINE:
      line = g->turbine == LUME_TURBINE_CONNECTED   ? "turbineComplete"
             : g->turbine == LUME_TURBINE_BLADES ? "turbineStage4"
             : g->turbine == LUME_TURBINE_DYNAMO ? "turbineStage3"
             : g->turbine == LUME_TURBINE_POLES ? "turbineStage2"
                               : "turbineStage1";
      break;
    case LOOK_GRAMOPHONE:
      line = "gramophoneLook";
      break;
    default:
      break;
    }
  }
  say(line);
  if((action==OPEN_SOLAR || action==OPEN_MAP || action==OPEN_PANTRY) && g->puzzle>=0) {
    dialogue_target=g->puzzle;g->puzzle=-1;
  } else if(g->puzzle>=0) puzzle_entry_speech(g->puzzle);
  if(action==READ_LETTER)after_clue="thought2";
  if(action==OPEN_METER) {
    if(!g->puzzles.completed[LUME_WIRES])popup_say("meter_wiring","needsWiring");
    else if(lume_game_power_level(g)==2)popup_say("meter_full","fullPower");
    else if(lume_game_power_level(g)==1)popup_say("meter_half","halfPower");
    else popup_say("meter_zero","zeroPower");
  }
  if(action==PRESS_POWER && !g->ended)popup_say("meter_low","meterShouldBeFull");
  if(action==READ_BOOK)popup_say("book1","book1Thought");
  if(action==READ_BOOK2)popup_say("book2","book2Thought");
  if(action==READ_BOOK3)popup_say("book3","book3Thought");
  if(action>=SHOW_PHOTO1 && action<=SHOW_PHOTO3) {
    const char *assets[]={"photo1","photo2","photo3"};
    popup_say(assets[action-SHOW_PHOTO1],assets[action-SHOW_PHOTO1]);
  }
  if(old==PANTRY) {
    if(action==TAKE_BINOCULARS)popup_say("binoculars","binocularsTaken");
    if(action==DISMANTLE_BIKE) {
      if(g->bike==1)popup_say("bike_thought","bikeThought");
      else if(g->bike==2)popup_say("dynamo_free","dynamoFree");
      else popup_say("poles_free","polesFree");
    }
    if(action==TAKE_DYNAMO && g->bike<2)popup_say("dynamo_fixed","dynamoFixed");
    if(action==TAKE_POLES && g->bike<3)popup_say("bike_thought","bikeThought");
  }
  if (old != g->scene) {
    lume_log("NAV start %d -> %d actor=%.2f,%.2f anchor=%d,%d", old, g->scene,
             actor_x, actor_y, feet[old][0], feet[old][1]);
    char name[48];
    if (navigation_override) snprintf(name, sizeof(name), "%s", navigation_override);
    else snprintf(name, sizeof(name), "nav_%d_%d", old, g->scene);
    navigation_override = NULL;
    if (old != PANTRY && g->scene != PANTRY)
      play_clip(name, 1);
    if (!animation.file)
      load_world(g);
    say(NULL);
    if(!animation.file && navigation_line) {
      const char *saved=navigation_line;navigation_line=NULL;say(saved);
    }
  }
  if (old == g->scene && (old_bike != g->bike || old_turbine != g->turbine ||
                          old_inventory != g->inventory)) {
    float x = actor_x, y = actor_y;
    load_world(g);
    actor_x = x;
    actor_y = y;
  }
  if (action == LOOK_VILLAGE && (g->inventory & (1u << BINOCULARS)) &&
      g->selected == BINOCULARS) {
    if(play_clip("anim_village",5))say(NULL);
    else say("villageSeen");
  }
  if (g->clue >= 0) cached_clue = -1;
  if (action == PLAY_GRAMOPHONE) {
    gramophone_pending=play_clip("anim_gramophone", 3);
  }
  if (g->ended && !was_ended) {
    lume_audio_music(true);
    play_clip("anim_ending", 4);
    say(NULL);
  }
  lume_persist_request(g);
  if (animation.file)
    clip_start = clip_tick_time = osGetTime();
  if (old != g->scene)
    lume_log("NAV setup_ms=%.3f",
             (svcGetSystemTick() - action_begin) / CPU_TICKS_PER_MSEC);
}
static bool start_authored_move(const LumeGame *g, int destination, int action) {
  const char *name = lume_motion_clip(g->scene, interaction.position, destination);
  /* camera2 has a shorter authored b_to_garden lead-in for savedMove. */
  if (g->scene == ROOF && interaction.position == 1 && action == GO_GARDEN)
    name = "walk_roof_bg";
  if (!name) return false;
  if (!play_clip(name, 6)) {
    lume_log("Authored movement asset missing: %s", name);
    return false;
  }
  lume_interaction_begin(&interaction,g->scene,destination,action);
  say(NULL);
  return true;
}
/* Complete the same stop-frame transaction used by AVM1 youStop: first
   retain the pose, then release the queued interaction exactly once. */
static void finish_authored_move(LumeGame *g, bool retained) {
  int action;
  LumeMoveResult result=lume_interaction_finish(&interaction,g->scene,retained,&action);
  if(result==LUME_MOVE_CANCELLED)return;
  if(result==LUME_MOVE_TRY_DOOR) {
    if(play_clip("walk_front_try",6))
      lume_interaction_begin(&interaction,g->scene,3,OPEN_FRONT);
    else {
      lume_interaction_reset(&interaction,FRONT);
      lume_actor_restore(FRONT);
    }
    return;
  }
  if(result==LUME_MOVE_FRONT_RETURN&&!g->puzzles.completed[LUME_DOOR])
    lume_actor_restore(FRONT);
  if(result==LUME_MOVE_STAIRS)navigation_override="nav_hall_stairs_after_walk";
  if(action>=0)dispatch(g,action);
}
static void request_action(LumeGame *g,int action) {
  if(action<0||action>=LUME_ACTION_COUNT||interaction.active)return;
  if(g->scene==ROOF&&action==TAKE_BLADES) {
    const char *line=lume_interaction_fence_line(g,interaction.position);
    if(line){say(line);return;}
  }
  int destination=lume_interaction_destination(g,interaction.position,action);
  if(destination!=interaction.position) {
    start_authored_move(g,destination,action);
    return;
  }
  dispatch(g,action);
}
static void request_floor_move(const LumeGame *g,int x) {
  if(interaction.active)return;
  int destination=lume_interaction_floor(g->scene,x);
  if(destination>=0)start_authored_move(g,destination,-1);
}
/* Completion flags are already set by the native puzzle. The popup is kept
   visible until the source feedback has actually been presented; afterwards
   release its reward/dialogue/navigation transaction exactly once. */
static void finish_puzzle_result(LumeGame *g) {
  int solved=puzzle_flow.mode,old=g->scene;
  const char *exit_line=lume_puzzle_exit_line(solved);
  bool exit_read=reading_replace&&reading.active&&reading.finished&&bubble&&
                 exit_line&&!strcmp(bubble,exit_line);
  puzzle_flow.mode=-1;result_frame=~0u;
  lume_animation_close(&result_timer);lume_animation_close(&meter_transition);say(NULL);
  lume_game_puzzle_result(g);
  if(solved==LUME_CUPBOARD) {g->clue=9;cached_clue=-1;after_clue="cupboardPuzzleComplete";}
  if(g->scene!=old && old==FRONT) {
    g->scene=FRONT;request_action(g,OPEN_FRONT);
  } else if(g->scene!=old) {
    char name[40];snprintf(name,sizeof name,"nav_%d_%d",old,g->scene);play_clip(name,1);
  }
  if(!animation.file)load_world(g);
  if(solved!=LUME_CUPBOARD && solved!=LUME_DOOR && !exit_read)say(exit_line);
  (void)lume_persist_request(g);
  if(animation.file)clip_start=clip_tick_time=osGetTime();
}
static void begin_puzzle_result(LumeGame *g,int mode,uint64_t now) {
  lume_puzzle_flow_begin(&puzzle_flow,mode,now);result_frame=0;visual_epoch++;
  if(mode==LUME_SOLAR && g->puzzles.completed[LUME_WIRES]) {
    puzzle_flow.power_preview=true;puzzle_flow.ticks=277;
  }
  // A quit during the visual delay must not save keysFound without its keys.
  // Match other pickups: ownership changes without opening a modal menu.
  if(mode==LUME_KEYS)g->inventory|=1u<<KEYS;
  (void)lume_persist_request(g);
  say(NULL);
  if(mode==LUME_SOLAR) {
    if(puzzle_flow.power_preview) {
      unsigned level=lume_game_power_level(g);
      const char *path=level==2?"romfs:/ui_meter_half_full.lan":"romfs:/ui_meter_zero_half.lan";
      lume_animation_close(&meter_transition);
      if(!lume_animation_open(&meter_transition,path))lume_log("Meter animation missing: %s",path);
      popup_say("power_fade",level==2?"fullPower":"halfPower");
    }
    else popup_say("solar_win","rotationPuzzleWin");
  }
  else if(mode==LUME_WIRES)popup_say("wires_correct",NULL);
  else if(mode==LUME_MAP)popup_say("map_correct",NULL);
  if(mode==LUME_WIRES)puzzle_flow.ticks=58;
  if(mode!=LUME_SOLAR && mode!=LUME_DOOR && mode!=LUME_CUPBOARD)
    if(!lume_animation_open(&result_timer,"romfs:/ui_result_timer.lan"))
      lume_log("Result timer overlay missing; retain authored duration");
  // The source timer can contain only invisible input covers; do not retain
  // its decode buffer when timing/input state already represents that layer.
  if(result_timer.file && !clip_has_art(&result_timer))lume_animation_close(&result_timer);
  puzzle_flow.started=osGetTime();speech_started=puzzle_flow.started;
}

static bool capture_clip_end(const LumeGame *g, bool navigation) {
  if (!lume_animation_frame(&animation, animation.frames - 1, NULL, 0))
    return false;
  bool pose_ok=lume_actor_capture_named_checked(g->scene, animation.x, animation.y, animation.width,
                     animation.height, animation.rgba,detail_clip_name);
  lume_log("CLIP_END asset=%s scene=%d source=%u:%u rect=%d,%d,%u,%u pose=%d",
    detail_clip_name,g->scene,animation.movie,animation.background,
    animation.x,animation.y,animation.width,animation.height,pose_ok);
  if (navigation) {
    /* The retained camera is now the loaded world. An inventory refresh must
       not mistake it for a scene change and overwrite a later b pose. */
    world_scene = g->scene;
    lume_interaction_reset(&interaction,g->scene);
    /* Retain both endpoint eyes even if the slider is currently closed.
       Opening 3D later must not reuse an older camera's right image. */
    if (animation.movie >= 4 || !open_movie(animation.movie))return false;
    lume_movie_set_strength(movie,256);
    if (!lume_movie_frame(movie, animation.background, movie_base[0],
                          movie_base[1]))
      return false;
    int8_t *field=malloc(76800);
    bool copied=field&&lume_movie_copy_disparity(movie,field);
    if(copied)lume_field_map(&world_field,field,320,240);else lume_field_close(&world_field);
    free(field);lume_movie_set_strength(movie,lume_stereo_strength());
    detail_world_mid=animation.movie;detail_world_frame=animation.background;
    memcpy(detail_world_base,movie_base[0],LUME_FRAME_BYTES);
    memcpy(world[0], movie_base[0], LUME_FRAME_BYTES);
    memcpy(world[2], movie_base[1], LUME_FRAME_BYTES);
    unsigned strength=lume_stereo_strength();lume_stereo_set_strength(256);
    for (int i = 0; i < 3; i += 2)
      lume_props_frame(world[i], animation.frames - 1, g->turbine,
                       (g->inventory & (1u << BLADES)) == 0 && g->turbine < LUME_TURBINE_BLADES,
                       i);
    lume_stereo_set_strength(strength);
    props_ready = false;
  }
  actor_x = feet[g->scene][0];
  actor_y = feet[g->scene][1];
  visual_epoch++;
  return true;
}
/* Emit every crossed source-frame event once, even when a display frame is
   skipped. Movement clips share the film clock instead of guessing footsteps. */
static void clip_audio(unsigned fr) {
  if (animation.cue_offsets || animation.cues) {
    unsigned start = clip_last == ~0u ? 0 : clip_last + 1;
    for (unsigned f = start; f <= fr; f++) {
      if (animation.cue_offsets) {
        for (unsigned j = animation.cue_offsets[f];
             j < animation.cue_offsets[f + 1]; j++)
          lume_audio_effect(animation.cue_ids[j]);
      } else if (animation.cues[f]) {
        lume_audio_effect(animation.cues[f]);
      }
    }
  }
}
/* The single spare-core budget belongs to display work until present
   completes. Decoder lookahead can then use the input/VBlank gap. */
static void movie_hint_after_present(void) {
 if(!clip_hint_pending)return;
 clip_hint_pending=false;
 if(!animation.file||clip_last==~0u||clip_last+1>=animation.frames)return;
 unsigned next_movie,next_background;
 if(lume_animation_camera_at(&animation,clip_last+1,&next_movie,&next_background)&&
    next_movie<4&&next_movie==(unsigned)movie_id)
  lume_movie_hint(movie,next_background,stereo_enabled);
}
static int clip_actor_parallax(const int8_t *map,unsigned mid,unsigned bg,unsigned frame) {
 /* A photographic dissolve is a screen-sized composite, not an actor.
    Moving it by the one-pixel actor fallback exposes the background at an
    edge and misaligns roof/tree silhouettes in the right eye. */
 if(!lume_depth_actor_crop(animation.width,animation.height)) {
   clip_depth_anchor.valid=false;return 0;
 }
 int x,y;
 if(!stereo_enabled||!map||!lume_depth_crop_foot(animation.rgba,animation.width,
      animation.height,animation.x,animation.y,&x,&y)) {
   clip_depth_anchor.valid=false;return 4;
 }
 unsigned source=mid==4?4+bg:mid;
 return lume_depth_follow(&clip_depth_anchor,lume_depth_ground(map,x,y),x,y,source,bg,frame);
}
static void ending_reading_sync(unsigned frame) {
 int current=lume_read_cue_at(ending_cues,ending_cue_count,frame);
 if(current!=ending_cue_current) {
   if(cinema_reading){lume_read_close(&reading);cinema_reading=false;visual_epoch++;}
   ending_cue_current=current;
   if(current>=0) {
     cinema_reading=lume_read_open(&reading,ending_cues[current].label);
     if(cinema_reading){reading_replace=false;reading.timed=true;lume_read_layout(&reading,320);visual_epoch++;}
     else lume_log("Ending dialogue unavailable: %s",ending_cues[current].label);
   }
 }
 if(cinema_reading&&current>=0) {
   unsigned page=lume_read_cue_page(&ending_cues[current],frame,reading.pages);
   if(page!=reading.page){reading.page=page;reading.presented=false;reading.serial++;visual_epoch++;}
 }
}
static bool movie_tick(void) {
  u64 tick = osGetTime();
  /* A walk/camera handoff must not silently jump over its authored poses.
     Adjust the clip clock under load instead of accumulating catch-up jumps. */
  unsigned fr;
  if(clip_kind==1||clip_kind==6) {
    uint64_t started=clip_start;
    fr=lume_animation_motion_frame(tick,&started,animation.fps,animation.frames,clip_last);
    clip_start=started;
  }
  else {
    if(clip_last==~0u)clip_start=tick;
    fr=lume_animation_clock_frame(tick,clip_start,animation.fps,animation.frames,clip_last);
  }
  if (fr >= animation.frames)
    return false;
  if (fr == clip_last && !stereo_dirty) {
    clip_tick_time = tick;
    return true;
  }
  if(stereo_enabled&&!clip_jobs_checked) {
    clip_jobs_checked=true;clip_jobs=lume_jobs_create();
    lume_log("CLIP_COMPOSE core=%d",clip_jobs?clip_jobs->core:-1);
  }
  u64 phase0 = svcGetSystemTick();
  if (!lume_animation_frame(&animation, fr, NULL, 0)) {
    lume_log("CLIP_FAIL asset=%s stage=rgba frame=%u/%u",detail_clip_name,fr,animation.frames);
    return false;
  }
  u64 phase1 = svcGetSystemTick();
  unsigned mid = animation.movie, bg = animation.background;
  if(mid>4){lume_log("CLIP invalid Lume camera id=%u",mid);return false;}
  lume_movie_set_strength(movie,lume_stereo_strength());
  if (clip_kind == 6) {
    rendered_mono=!stereo_enabled;
    /* The camera stays still for a/b movement. Reuse its decoded eye images;
       only the small RGBA character crop is inflated and blended per frame. */
    /* Static props are shared by the two eyes; retain their original
       placement while composing the independently owned eye buffers. */
    for(int i=0;i<3;i+=2) {
      if(i&&rendered_mono)continue;
      memcpy(rendered[i],i?lume_field_view(&world_field,world[0],world[2]):world[0],LUME_FRAME_BYTES);
      if(props_ready)lume_blit_rgba_eye(rendered[i],props,320,240,0,0,i);
    }
    if(!lume_clip_compose(clip_jobs,&animation,fr,rendered[0],rendered[2],
                         rendered[0],rendered[2],stereo_enabled,false,0,false,
                         clip_actor_parallax(world_field.valid?world_field.map:NULL,
                           detail_world_mid,detail_world_frame,fr)))return false;
    if (clip_last != ~0u && fr > clip_last + 1)
      clip_dropped += fr - clip_last - 1;
    clip_drawn++;
    frame_serial++;
    clip_audio(fr);
    clip_last = fr;
    stereo_dirty=false;
    clip_tick_time = tick;
    return true;
  }
  if (mid < 4) {
    if (cinema_plate >= 0) {
      lume_movie_sync(movie);
      movie->last=~0u;
    }
    cinema_plate = -1;
    if (!open_movie(mid))
      return false;
    /* A queued frame must not stall touch/Skip/audio polling. Keep the last
       image while the producer owns the decoder; the outer loop still yields
       at VBlank, so this is a readiness check rather than a busy-spin. */
    /* Readiness polling must not reset the presentation deadline. Doing so
       adds another VBlank interval to every slightly late decoded frame. */
    if(!lume_movie_ready(movie)) {
      if(clip_kind==3)clip_start+=tick-clip_tick_time;
      clip_tick_time=tick;return true;
    }
    if (!lume_movie_frame(movie,bg,movie_base[0],stereo_enabled?movie_base[1]:NULL)) {
      if (lume_cache_pending(&movie->cache)) {
        if(clip_kind==3 && gramophone_clock.active)
          gramophone_clock.started+=tick-clip_tick_time;
        if(clip_kind==3)clip_start+=tick-clip_tick_time;
        clip_tick_time = tick;
        return true;
      }
      lume_log("CLIP_FAIL asset=%s stage=movie source=%u frame=%u",detail_clip_name,mid,bg);
      return false;
    }
  } else if(cinema_plate!=(int)bg || stereo_dirty) {
    if(!lume_plate_cache_frame(&plate_cache,bg,stereo_enabled,
                              movie_base[0],movie_base[1],read_file)) {
      lume_log("Cinema plate missing or invalid: %u",bg);return false;
    }
    cinema_plate=bg;
  }
  rendered_mono=!stereo_enabled;
  const LumeAnimation *effect=NULL;
  if(clip_kind==3) {
    gramophone_actor_frame(fr,tick);
    if(gramophone_clock.active && gramophone_clock.frame!=~0u && gramophone_effect.file &&
       lume_animation_frame(&gramophone_effect,gramophone_clock.frame,NULL,0))effect=&gramophone_effect;
  }
  u64 phase2 = svcGetSystemTick();
  bool draw_props=clip_kind==1 && active_game;
  if(draw_props)lume_props_set_visibility(active_game->puzzles.completed[LUME_MAP],!active_game->puzzles.completed[LUME_SOLAR]);
  /* Sample the frame just published, never a lookahead or the preceding
     camera. Freeze one scalar before launching either eye's composition. */
  const int8_t *actor_field=stereo_enabled?(mid<4?
    lume_movie_disparity_frame(movie,bg):lume_plate_cache_disparity(&plate_cache,bg)):NULL;
  int actor_quarter_pixels=clip_actor_parallax(actor_field,mid,bg,fr);
  if(!lume_clip_compose_layers(clip_jobs,&animation,fr,movie_base[0],movie_base[1],
     rendered[0],rendered[2],stereo_enabled,draw_props,draw_props?active_game->turbine:0,
     draw_props && (active_game->inventory&(1u<<BLADES))==0 && active_game->turbine<LUME_TURBINE_BLADES,effect,actor_quarter_pixels))return false;
  u64 phase3=svcGetSystemTick();
  if(phase3-phase0>=80*CPU_TICKS_PER_MSEC) {
    lume_log("CLIP_SLOW asset=%s frame=%u movie=%u background=%u rgba=%ux%u "
      "strength=%u lan_ms=%.3f video_ms=%.3f compose_ms=%.3f total_ms=%.3f",
      detail_clip_name,fr,mid,bg,animation.width,animation.height,lume_stereo_strength(),
      (phase1-phase0)/(double)CPU_TICKS_PER_MSEC,(phase2-phase1)/(double)CPU_TICKS_PER_MSEC,
      (phase3-phase2)/(double)CPU_TICKS_PER_MSEC,(phase3-phase0)/(double)CPU_TICKS_PER_MSEC);
  }

  if(clip_kind==4&&fr>=LUME_LOGO_ENDING_FIRST_FRAME&&stereo_enabled)
    lume_logo_ending_front(rendered[2],lume_stereo_strength());
  if(clip_kind==4)ending_reading_sync(fr);
  frame_serial++;
  static u64 phase_sum[4];
  static unsigned phase_n;
  u64 phase4 = svcGetSystemTick();
  phase_sum[0] += phase1 - phase0;
  phase_sum[1] += phase2 - phase1;
  phase_sum[2] += phase4 - phase3;
  phase_sum[3] += phase3 - phase2;
  if (++phase_n == 60) {
    if(movie->mpeg){
      const LumeMovieStats *s=&movie->stats;
      double frames=s->decoded_frames?s->decoded_frames:1,conversions=s->conversions?s->conversions:1;
      double stereo=s->stereo_frames?s->stereo_frames:1,depth=s->depth_frames?s->depth_frames:1;
      /* Extend the existing periodic line instead of adding another SD-file
         open/close to every profiling interval. Timings are wall intervals. */
      lume_log("MOVIE prepare=%.3f video=%.3f props=%.3f compose=%.3f "
        "core=%d decoded=%lu depth_jobs=%lu stereo_jobs=%lu rewinds=%lu cached=%lu "
        "depth_skipped=%lu codec=%.3f rgb=%.3f depth=%.3f depth_wait=%.3f stereo=%.3f "
        "ahead_requests=%lu ahead_hits=%lu ahead_misses=%lu ahead_busy=%lu ahead_wait=%.3f y2r=%lu direct_rgb=%lu sw_rgb=%lu y2r_mae=%.3f y2r_error=%s y2r_rc=%08lX colour_samples=%lu colour_hw_ms=%.3f colour_sw_ms=%.3f",
        phase_sum[0]/(60.*CPU_TICKS_PER_MSEC),phase_sum[1]/(60.*CPU_TICKS_PER_MSEC),
        phase_sum[2]/(60.*CPU_TICKS_PER_MSEC),phase_sum[3]/(60.*CPU_TICKS_PER_MSEC),
        s->worker_core,(unsigned long)s->decoded_frames,(unsigned long)s->depth_jobs,
        (unsigned long)s->stereo_jobs,(unsigned long)s->rewinds,(unsigned long)s->cached_frames,(unsigned long)s->depth_skipped,
        s->codec_ticks/(frames*CPU_TICKS_PER_MSEC),s->convert_ticks/(conversions*CPU_TICKS_PER_MSEC),
        s->depth_ticks/(depth*CPU_TICKS_PER_MSEC),s->depth_wait_ticks/(depth*CPU_TICKS_PER_MSEC),
        s->stereo_ticks/(stereo*CPU_TICKS_PER_MSEC),
        (unsigned long)s->lookahead_requests,(unsigned long)s->lookahead_hits,
        (unsigned long)s->lookahead_misses,(unsigned long)s->lookahead_busy_polls,s->lookahead_wait_ticks/((s->lookahead_hits+s->lookahead_misses?s->lookahead_hits+s->lookahead_misses:1)*CPU_TICKS_PER_MSEC),
        (unsigned long)s->y2r_frames,(unsigned long)s->y2r_direct_frames,(unsigned long)s->y2r_fallbacks,s->y2r_mae_milli/1000.,s->y2r_failure?s->y2r_failure:"none",(unsigned long)(uint32_t)s->y2r_result,(unsigned long)s->colour_samples,
        s->colour_hw_ticks/((s->colour_samples?s->colour_samples:1)*CPU_TICKS_PER_MSEC),
        s->colour_sw_ticks/((s->colour_samples?s->colour_samples:1)*CPU_TICKS_PER_MSEC));
    }else {
      lume_log("MOVIE prepare=%.3f video=%.3f props=%.3f compose=%.3f",
        phase_sum[0]/(60.*CPU_TICKS_PER_MSEC),phase_sum[1]/(60.*CPU_TICKS_PER_MSEC),
        phase_sum[2]/(60.*CPU_TICKS_PER_MSEC),phase_sum[3]/(60.*CPU_TICKS_PER_MSEC));
    }
    const LumeGpuVideoStats *gpu=lume_gpu_video_stats();
    if(gpu->attempted) {
      double count=gpu->frames?gpu->frames:1;
      lume_log("GPU_VIDEO state=%s frames=%u upload_ms=%.3f submit_ms=%.3f transfer_wait_ms=%.3f reused=%u transfers=%u",
        gpu->state,gpu->frames,gpu->upload_ticks/(count*CPU_TICKS_PER_MSEC),
        gpu->render_ticks/(count*CPU_TICKS_PER_MSEC),gpu->copy_ticks/(count*CPU_TICKS_PER_MSEC),gpu->reused,gpu->transfers);
    }
    unsigned full,partial,reused,saved_rows;
    lume_present_filter_stats(&full,&partial,&reused,&saved_rows);
    lume_log("PRESENT_FILTER full=%u partial=%u reused=%u saved_rows=%u",full,partial,reused,saved_rows);
    memset(phase_sum, 0, sizeof(phase_sum));
    phase_n = 0;
    lume_log("LAN lz4=%u zlib=%u reused=%u delta=%u",animation.lan_state.lz4_blocks,
      animation.lan_state.zlib_blocks,animation.lan_state.reused_blocks,animation.lan_state.delta_blocks);
  }
  if (clip_last != ~0u && fr > clip_last + 1)
    clip_dropped += fr - clip_last - 1;
  clip_drawn++;
  clip_audio(fr);
  clip_last = fr;
  clip_hint_pending=fr+1<animation.frames;
  stereo_dirty=false;
  clip_tick_time = tick;
  return true;
}
static bool native_menu_controls(LumeGame *g,u32 *down,u32 *held,bool stable,bool *quit) {
 (void)quit;
 if(manual_save_ticket){*down=*held=0;return true;}
 static bool release_input;
 if(release_input){
  if(!*held)release_input=false;
  *down=0;*held=0;return true;
 }
 if(!native_menu.mode&&!(*down&(KEY_START|KEY_SELECT)))return false;
 bool was_open=native_menu.mode!=LUME_MENU_CLOSED;
 u32 d=*down;unsigned keys=0;
 if(d&KEY_START)keys|=LM_START;
 if(d&KEY_SELECT)keys|=LM_SELECT;
 if(d&(KEY_B|KEY_X))keys|=LM_BACK;
 if(d&KEY_A)keys|=LM_OK;
 if(d&(KEY_DUP|KEY_CPAD_UP))keys|=LM_UP;
 if(d&(KEY_DDOWN|KEY_CPAD_DOWN))keys|=LM_DOWN;
 if(d&(KEY_DLEFT|KEY_CPAD_LEFT))keys|=LM_LEFT;
 if(d&(KEY_DRIGHT|KEY_CPAD_RIGHT))keys|=LM_RIGHT;
 touchPosition touch={0};
 if(d&KEY_TOUCH){keys|=LM_TOUCH;hidTouchRead(&touch);}
 unsigned action=lume_menu_input(&native_menu,g,keys,touch.px,touch.py,stable);
 if(native_menu.mode&&!native_menu_art.attempted&&!lume_menu_art_load(&native_menu_art)){
  strcpy(native_menu.notice,"Generate original menu art");native_menu.serial++;
 }
 if(action==LUME_MENU_SAVE||action==LUME_MENU_QUIT){
  manual_save_ticket=lume_persist_request(g);manual_save_quit=action==LUME_MENU_QUIT;
  strcpy(native_menu.notice,manual_save_ticket?"Saving...":"Save failed - try again");native_menu.serial++;
 }
 if(was_open&&!native_menu.mode)release_input=true;
 *down=0;*held=0;return true;
}
int main(void) {
  lume_log_io_init("sdmc:/3ds/lume_puzzles/runtime.log");
  lume_runtime_set_flags_directory("sdmc:/3ds/lume_puzzles");
  lume_runtime_set_asset_directory("romfs:");
  lume_ui_set_user_directory("sdmc:/3ds/lume_puzzles");
  lume_ui_set_fallback(lume_font_alpha,lume_draw_text);
  if (!vfp_set_fz_dn())
    return 1;
  osSetSpeedupEnable(false);
  avatar_scales_init();
  gfxInitDefault();
  gfxSet3D(true);
  bool romfs_ready = R_SUCCEEDED(romfsInit());
  if (romfs_ready) {
    lume_actor_init();
    map_ink_ready = read_file("romfs:/map_ink.bin", map_ink, sizeof(map_ink)) &&
                    !memcmp(map_ink, "INK2", 4);
  }
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/lume_puzzles", 0777);
  if(!lume_persist_start())lume_log("SAVE worker unavailable");
  /* Old hardware keeps its normal clock. New hardware can use the official
     speedup; an empty compat_cpu.flag keeps a comparable baseline if needed. */
  FILE *compat_cpu=fopen("sdmc:/3ds/lume_puzzles/compat_cpu.flag","rb");
  bool speedup_requested=compat_cpu==NULL,new_hardware=false;
  if(compat_cpu)fclose(compat_cpu);
  osSetSpeedupEnable(speedup_requested);
  Result model_result=APT_CheckNew3DS(&new_hardware);
#ifdef __OPTIMIZE__
  const unsigned build_optimized=1;
#else
  const unsigned build_optimized=0;
#endif
#ifdef __3DS__
  const unsigned build_cache_worker=1;
#else
  const unsigned build_cache_worker=0;
#endif
  lume_log("CPU new_hw=%d model_rc=%08lx speedup_requested=%d optimized=%u cache_worker=%u",new_hardware,
           (unsigned long)model_result,speedup_requested,build_optimized,build_cache_worker);
  lume_log(
      "Lume startup; authored scene motion and original presentation; video_fix=gpu_video_stage_v6_tiles");
  if (!romfs_ready) {
    consoleInit(GFX_BOTTOM, NULL);
    printf("Lume: RomFS could not be opened.\n");
    while (aptMainLoop()) {
      hidScanInput();
      if (hidKeysDown() & KEY_START)
        break;
      gspWaitForVBlank();
    }
    lume_persist_stop();lume_log_io_drain();
    lume_gpu_video_close();gfxExit();
    return 1;
  }
  char path[128];
  for (int i = 0; i < 7; i++) {
    snprintf(path, sizeof(path), "romfs:/%s.rgb", lume_asset_names[i]);
    read_file(path, backgrounds[i], LUME_FRAME_BYTES);
  }
  solar_completed_ready=read_file("romfs:/solar_completed.rgb",solar_completed,sizeof solar_completed);
  read_file("romfs:/solar_pattern.rgb", pattern, sizeof(pattern));
  solar_tiles_ready = read_file("romfs:/solar_tiles.bin", solar_tiles,
                                 sizeof(solar_tiles)) &&
                      !memcmp(solar_tiles, "SLR1", 4);
  lume_log("Original solar atlas: %s", solar_tiles_ready ? "ready" : "missing");
  read_file("romfs:/digits_door.alpha", digits[0], sizeof(digits[0]));
  read_file("romfs:/digits_cupboard.alpha", digits[1], sizeof(digits[1]));
  avatar_ready = read_file("romfs:/avatar.rgba", avatar, sizeof(avatar));
  if(!lume_close_load())lume_log("Close-control UI unavailable");
  lume_locale_init();
  if (!lume_audio_init())
    lume_log("Audio unavailable; physical hardware requires dspfirm.cdc");
  lume_audio_levels(lume_locale.music,lume_locale.effects);
  if (!lume_props_init())
    lume_log("Some prop textures could not be loaded");
  restart_title:;
  LumeGame game, saved_game;
  bool has_save = lume_game_load(&saved_game, "sdmc:/3ds/lume_puzzles/save.lum");
  int choice = show_title(has_save);
  if (choice == TITLE_QUIT) {
    lume_jobs_close(clip_jobs);clip_jobs=NULL;
    /* Browsing the title/credits must never create or overwrite progress. */
    lume_props_quit();
    lume_audio_quit();lume_read_release(&reading);
    lume_persist_stop();lume_log_io_drain();
    romfsExit();
    lume_gpu_video_close();gfxExit();
    return 0;
  }
  bool loaded = has_save && (choice == TITLE_CONTINUE || choice == TITLE_REPLAY);
  bool replay_intro = choice == TITLE_REPLAY;
  if (loaded)
    game = saved_game;
  else {
    lume_game_new(&game, osGetTime());
    game.scene = ROOF;
  }
  if(loaded)lume_persist_seed(&game);else (void)lume_persist_request(&game);
  active_game = &game;
  lume_view_init(&viewport,osGetTime());
  lume_log("Title choice=%d saved=%d intro=%d", choice, loaded,
           !loaded || replay_intro);
  load_world(&game);
  SceneAssets assets = {0};
  for (int i = 0; i < 7; i++)
    assets.background[i] = backgrounds[i];
  assets.pattern = pattern;
  assets.solar_tiles = solar_tiles_ready ? solar_tiles : NULL;
  assets.source_lock_art = false;
  assets.door_digits = digits[0];
  assets.cupboard_digits = digits[1];
  bool debug = false, quit = false, ending_hold = game.ended,return_to_title=false;
  ending_card_open=false;manual_save_ticket=ending_save_ticket=0;
  LumePuzzles debug_state;
  lume_reset_all(&debug_state, 12345);
  int mode = 0, loaded_mode = -1;
  if (!loaded || replay_intro)
    play_clip("anim_intro", 2);
  while (aptMainLoop() && !quit) {
    u64 perf_start = svcGetSystemTick();
    hidScanInput();
    u32 down = hidKeysDown(), held = hidKeysHeld();
    reading_block_touch(&down,&held);
    u64 now = osGetTime();
    if(manual_save_ticket) {
      int saved=lume_persist_status(manual_save_ticket);
      if(saved){strcpy(native_menu.notice,saved>0?"Saved":"Save failed - try again");native_menu.serial++;
        if(saved>0&&manual_save_quit)quit=true;
        manual_save_ticket=0;manual_save_quit=false;}
      down=held=0;
    }
    stereo_poll();
    if(!ending_hold&&puzzle_flow.mode<0&&!reading_instruction&&native_menu_controls(&game,&down,&held,!animation.file&&puzzle_flow.mode<0,&quit))
      viewport.tick=now;
    else viewport_input(down,held,now);
    lume_audio_tick();
    u64 perf_audio = svcGetSystemTick();
    if(!animation.file || clip_kind!=3)gramophone_tick(now,&game);
    if(cinema_reading) {
      /* Source-timed subtitles advance with the film, never with Touch/A.
         Keep B available to the existing whole-film skip handler. */
      if(down&KEY_TOUCH)reading_touch_latch=true;
      down&=~(KEY_A|KEY_TOUCH);held&=~KEY_TOUCH;
    }
    if(ending_hold&&!animation.file) {
      if(!ending_card_open) {
        say(NULL);ending_card_open=true;reading_replace=false;
        lume_read_open(&reading,"page_ending");lume_read_layout(&reading,320);visual_epoch++;
      }
      bool menu=(down&(KEY_START|KEY_B))!=0;
      bool next=(down&KEY_A)!=0;
      if(down&KEY_TOUCH){touchPosition touch;hidTouchRead(&touch);menu=touch.py>=210;next=!menu;reading_touch_latch=true;}
      if(next) {
        if(reading.active&&!reading.finished){if(lume_read_step(&reading,false))visual_epoch++;}
        else menu=true;
      }
      if(menu&&!ending_save_ticket)ending_save_ticket=lume_persist_request(&game);
      if(ending_save_ticket){int saved=lume_persist_status(ending_save_ticket);
        if(saved>0){return_to_title=true;break;}
        if(saved<0){ending_save_ticket=0;lume_log("ENDING save failed; menu retained");}}
      down=0;held&=~KEY_TOUCH;
    }
    if (animation.file) {
      int kind = clip_kind;
      bool skip_touch=skip_input(kind,&down,&held);
      if (skip_touch || (down & KEY_B) || ((down & KEY_X) && (kind == 1 || kind == 3)) ||
          !movie_tick()) {
        u64 exit_begin = svcGetSystemTick();
        bool retained =
            (kind == 1 || kind == 3 || kind == 6) &&
            capture_clip_end(&game, kind == 1);
        close_clip();
        if (kind == 4)
          ending_hold = true;
        else if (!retained && kind != 5)
          load_world(&game);
        if (kind == 6) {
          finish_authored_move(&game, retained);
          down = 0; /* Do not deliver the movement click to a newly opened puzzle. */
        }
        down=0;
        if(kind==1 && navigation_line) {
          const char *line=navigation_line;navigation_line=NULL;say(line);
        }
        if(kind==2)say("intro");
        if (kind == 5)
          say("villageSeen");
        if (kind == 1)
          lume_log("NAV exit_ms=%.3f retained=%d",
                   (svcGetSystemTick() - exit_begin) / CPU_TICKS_PER_MSEC, retained);
      }
    }
    if((down&KEY_X)&&!animation.file&&!ending_hold&&puzzle_flow.mode<0&&!native_menu.mode){
      int close=lume_close_target(&game,debug,reading_instruction);
      if(close==LUME_CLOSE_HELP){lume_read_close(&reading);reading_instruction=false;visual_epoch++;}
      else if(close==LUME_CLOSE_PUZZLE){debug=false;game.puzzle=-1;say(NULL);}
      else if(close==LUME_CLOSE_CLUE)close_clue(&game);
      else if(close==LUME_CLOSE_PANTRY)dispatch(&game,GO_HALL);
      if(close!=LUME_CLOSE_NONE){down=0;held&=~KEY_TOUCH;reading_touch_latch=true;}
    }
    speech_tick(now);
    document_input(&game,&down,&held);
    reading_input(&game,&down,&held);
    if(!animation.file && puzzle_flow.mode>=0) {
      if(reading_result_ready(now)) {finish_puzzle_result(&game);down=0;}
      else {
        unsigned f=lume_puzzle_flow_frame(&puzzle_flow,now);
        if(f!=result_frame){result_frame=f;visual_epoch++;}
        down=0;
      }
    }
    if(!animation.file && bubble && speech_modal && puzzle_flow.mode<0 &&
       (down&(KEY_A|KEY_B|KEY_TOUCH))) {
      if(speech_presented)dismiss_speech(&game,(down&KEY_B)==0);
      down &= ~(KEY_A|KEY_B|KEY_TOUCH);
    }
    if (!animation.file) {
      if ((held & (KEY_L | KEY_R)) == (KEY_L | KEY_R) &&
          (down & (KEY_L | KEY_R))) {
        debug = !debug;
        say(NULL);
        lume_interaction_cancel(&interaction);
              loaded_mode = -1;
      }
      bool puzzle = debug || game.puzzle >= 0;
      if (puzzle) {
        if (!debug)
          mode = game.puzzle;
        LumePuzzles *state = debug ? &debug_state : &game.puzzles;
        bool reset = false, submit = down & KEY_A;
        if (down & KEY_B) {
          if (debug)
            debug = false;
          else
            game.puzzle = -1;
          say(NULL);submit=reset=false;down=0;
        }
        if (debug && down & KEY_L)
          mode = (mode + 6) % 7;
        if (debug && down & KEY_R)
          mode = (mode + 1) % 7;


        if (down & KEY_TOUCH) {
          touchPosition t;
          viewport_touch_read(&t);
          if(puzzle_help_open(mode,t.px,t.py,&down,&held)) {
            submit=reset=false;
          } else if (t.px < 29 && t.py >= 14 && t.py < 43) {
            if (debug)
              debug = false;
            else
              game.puzzle = -1;
            say(NULL);submit=reset=false;down=0;
          } else {
            int x = t.px, y = t.py;

            int a = lume_touch(state, mode, x, y);
            if (a >= 0) {
              if(!speech_modal)say(NULL);
              status = 0;
              if (mode == LUME_WIRES && (a == 0 || a == 200))
                lume_audio_effect(497);
              else if (a == 0 && mode == LUME_KEYS)
                lume_audio_effect(655);
              else if (a == 0 && (mode == LUME_DOOR || mode == LUME_CUPBOARD))
                lume_audio_effect(627);
              else if (a == 0 && mode == LUME_MAP)
                lume_audio_effect(640);
            }
            if (a == 100)
              submit = true;
            if (a == 200)
              reset = true;
            if (mode == LUME_PIANO && a >= 1 && a <= 8) {
              piano_press(a-1,now);
              lume_audio_note(a);
              if (state->piano.count == 5)
                submit = true;
            }
            if (mode == LUME_SOLAR && a == 0)
              submit = true;
          }
        }
        if (reset) {
          if(mode==LUME_PIANO)piano_press(8,now);
          say(NULL);
          lume_reset_one(state, mode, now);
          status = 0;
        }
        if (submit) {
          if (mode == LUME_WIRES)
            lume_audio_effect(497);
          status = lume_submit(state, mode) ? 1 : -1;
          if (status > 0 && mode == LUME_PIANO)
            lume_audio_effect(1);
          else if (status > 0 && mode == LUME_WIRES)
            lume_audio_effect(500);
          if (!debug && state->completed[mode]) {
            begin_puzzle_result(&game,mode,now);
          } else if(!debug && status<0) {
            if(mode==LUME_KEYS)popup_say("keys_incorrect","keyPuzzleIncorrect");
            else if(mode==LUME_MAP)popup_say("map_incorrect","mapPuzzleIncorrect");
            else if(mode==LUME_WIRES) {popup_say("wires_incorrect","puzzleIncomplete");speech_modal=false;}
          }

        }
      } else if (!ending_hold) {
        if (down & KEY_B) {
          if (bubble)
            say(NULL);
          else if (game.clue >= 0) {
            close_clue(&game);
            
          } else if (game.scene == PANTRY || game.scene == POWER)
            dispatch(&game, GO_HALL);
        }
        if (down & KEY_TOUCH) {
          touchPosition t;
          viewport_touch_read(&t);
          int action = -1;
          bool touch_consumed=game.clue>=0 || bubble!=NULL;
          if (game.clue >= 0) {
            if (game.clue == 8 &&
                (t.px - 158) * (t.px - 158) + (t.py - 172) * (t.py - 172) <
                    24 * 24)
              action = PRESS_POWER;
            else if (t.px < 29 && t.py < 43) {
              close_clue(&game);
            }
            else if (game.clue == 9) {
              int x=t.px,y=t.py;

              action = lume_game_clue_hotspot(&game,x,y);
              if(action<0) { close_clue(&game);  }
            }
            else if (game.clue != 8) { close_clue(&game); }
          } else if (game.scene == PANTRY && t.px < 29 && t.py>=14 && t.py<43) {
            action=GO_HALL;
          } else if (bubble) {
            say(NULL);
          } else
            action = lume_game_hotspot(&game, t.px, t.py);
          if (action >= 0) {
            request_action(&game, action);
          } else if (!touch_consumed && game.clue < 0 && !bubble && t.py > 145 &&
                     t.py < 225) {
            request_floor_move(&game,t.px);
          }
        }
      }
      piano_tick(now,(debug || game.puzzle>=0) && mode==LUME_PIANO);
      if(debug || game.puzzle>=0) {
        load_puzzle_art(mode);
        LumePuzzles *p=debug?&debug_state:&game.puzzles;
        p->wire.preview_x=p->wire.preview_y=-1;
        if(mode==LUME_WIRES && p->wire.selected>=0 && (held&KEY_TOUCH)) {
          touchPosition t;viewport_touch_read(&t);int x=t.px,y=t.py;

          p->wire.preview_x=x;p->wire.preview_y=y;
        }
      } else {load_puzzle_art(-1);loaded_mode=-1;}
      if (!animation.file &&
          scene_needs_redraw(&game, &debug_state, debug, mode, false, 0, 0)) {
        detail_staged=false;detail_has_actor=false;
        rendered_mono=!stereo_enabled;
        frame_serial++;
        if (ending_hold) {
          if (!credits_loaded)
            for (int i = 0; i < 3; i += 2) {
              snprintf(path, sizeof(path), "romfs:/ending_%c.rgb",
                       i == 2 ? 'R' : 'L');
              if (!read_file(path, rendered[i], LUME_FRAME_BYTES)) {
                open_movie(3);
                lume_movie_sync(movie);
                movie->last=~0u;
                lume_movie_frame(movie, movie->frames - 1, rendered[1],
                                 rendered[2]);
                memcpy(rendered[0], rendered[1], LUME_FRAME_BYTES);
              }
            }
          if(!credits_loaded) {
            lume_field_load(&ending_field,"romfs:/ending_L.rgb",320,240);
            ending_field.image=malloc(LUME_FRAME_BYTES);
            if(ending_field.image)memcpy(ending_field.image,rendered[2],LUME_FRAME_BYTES);
          }
          if(ending_field.image)lume_field_draw(&ending_field,rendered[0],ending_field.image,rendered[2],lume_stereo_strength());
          credits_loaded = true;
        } else if (debug || game.puzzle >= 0) {
          if (!debug)
            mode = game.puzzle;
          if (loaded_mode != mode) {
            load_puzzle_stereo(mode);
            lume_animation_close(&lock_progress);
            if(mode==LUME_CUPBOARD && !lume_animation_open(&lock_progress,"romfs:/ui_cupboard_progress.lan"))
              lume_log("Cupboard progress overlay missing");

            loaded_mode = mode;
          }
          LumePuzzles *state = debug ? &debug_state : &game.puzzles;
          bool source_board=mode==LUME_WIRES || mode==LUME_KEYS || mode==LUME_DOOR || mode==LUME_CUPBOARD;
          rendered_mono=!stereo_enabled;
          for (int i = 0; i < 3; i += 2) {
            if(i && rendered_mono) {
              /* With the slider closed, the right eye need not be rebuilt. */
              continue;
            }
            SceneAssets view = assets;
            view.source_lock_art=(mode==LUME_DOOR || mode==LUME_CUPBOARD) && puzzle_art.file;
            /* Source boards warp after all changed state art is composed,
               so both eyes show the same current keys, digits and wires. */
            if (i && !source_board) {
              view.background[mode] = lume_field_view(&puzzle_field,stereo[0],stereo[1]);
              view.pattern = lume_field_view(&pattern_field,stereo_pattern[0],stereo_pattern[1]);
            }

            view.map_ink = map_ink_ready ? map_ink : NULL;

            if(source_board)draw_source_puzzle(rendered[i],&view,state,mode,status,i);
            else {lume_render(rendered[i], &view, state, mode, status);draw_puzzle_art(rendered[i],state,mode);}
            if(mode==LUME_PIANO)draw_piano_presses(rendered[i]);
            /* The authored page already contains its close button. Drawing
               another icon here doubles its ink and changes its size. */

            if(puzzle_flow.mode==LUME_SOLAR) {
              if(solar_completed_ready){
                if(i)lume_field_dynamic(&puzzle_field,solar_completed,rendered[i],lume_stereo_strength());
                else memcpy(rendered[i],solar_completed,LUME_FRAME_BYTES);
              }
              if(puzzle_flow.power_preview && result_frame>=15) {
                if(result_frame<266) {
                  LumeGame preview=game;preview.clue=8;
                  load_clue_images(&preview);memcpy(rendered[i],i?lume_field_view(&clue_field,clue_images[0],clue_images[2]):clue_images[0],LUME_FRAME_BYTES);
                  if(meter_transition.file) {
                    unsigned frame=result_frame-15;
                    if(frame>=meter_transition.frames)frame=meter_transition.frames-1;
                    memcpy(meter_frame,clue_images[0],sizeof meter_frame);
                    if(lume_animation_frame(&meter_transition,frame,meter_frame,0)) {
                      if(i)lume_field_dynamic(&clue_field,meter_frame,rendered[i],lume_stereo_strength());
                      else memcpy(rendered[i],meter_frame,sizeof meter_frame);
                    }
                  }
                } else {
                  lume_game_render(rendered[i],i?lume_field_view(&world_field,world[0],world[2]):world[0],&game,0);
                  if(props_ready)lume_blit_rgba_eye(rendered[i],props,320,240,0,0,i);
                  if(!i)detail_stage(rendered[i],0,game.scene!=PANTRY);
                  draw_avatar(rendered[i],i,&game);
                }
              }
            }
            if(!i){detail_mode=mode;detail_state=state;if(viewport.scale>1024){memcpy(detail_reference,rendered[i],LUME_FRAME_BYTES);detail_reference_ready=true;}else detail_reference_ready=false;}
            if(!i&&!detail_has_actor)detail_stage(rendered[i],0,false);
            if(speech_ready)draw_speech(rendered[i]);
            if(!i)detail_stage(rendered[i],1,false);
            if(result_timer.file && puzzle_flow.mode>=0)
              lume_animation_frame(&result_timer,result_frame<52?result_frame:51,rendered[i],0);
          }
        } else {
          for (int i = 0; i < 3; i += 2) {
            if(i && rendered_mono)continue;
            if (game.clue >= 0) {
              load_clue_images(&game);
              memcpy(rendered[i],i?lume_field_view(&clue_field,clue_images[0],clue_images[2]):clue_images[0],LUME_FRAME_BYTES);
              if(!i)detail_stage(rendered[i],0,false);
            } else {
              lume_game_render(rendered[i],i?lume_field_view(&world_field,world[0],world[2]):world[0],&game,0);
              if (props_ready)lume_blit_rgba_eye(rendered[i],props,320,240,0,0,i);
              draw_gramophone(rendered[i],i);
              if(!i)detail_stage(rendered[i],0,game.scene!=PANTRY);
              draw_avatar(rendered[i], i, &game);
            }

            if(!i){detail_mode=-1;detail_state=NULL;if(viewport.scale>1024){memcpy(detail_reference,rendered[i],LUME_FRAME_BYTES);detail_reference_ready=true;}else detail_reference_ready=false;}
            if (speech_ready)draw_speech(rendered[i]);
            if(!i)detail_stage(rendered[i],1,false);

          }
        }
      }
    }
    if(lume_document_sync(&document_reading,&reading,&game))visual_epoch++;
    if(!lume_reader_supported(&game))lume_reader_close(&clue_reader);
    u64 perf_draw = svcGetSystemTick();
    static unsigned presented = ~0u;
    static unsigned presented_view=~0u,presented_reading=~0u,presented_menu=~0u;
    bool shown=presented!=frame_serial || presented_view!=viewport.serial || presented_reading!=reading.serial || presented_menu!=native_menu.serial;
    if (shown) {
      present();
      gfxSwapBuffers();
      presented = frame_serial;presented_view=viewport.serial;presented_reading=reading.serial;presented_menu=native_menu.serial;
      if(!animation.file && speech_ready && speech_ack)speech_presented=true;
      if(!animation.file && puzzle_flow.mode>=0)puzzle_flow.shown=result_frame;
    }
    movie_hint_after_present();
    u64 perf_present = svcGetSystemTick();
    gspWaitForVBlank();
    lume_profile_frame(perf_start, perf_audio, perf_draw, perf_present,
                       svcGetSystemTick(), game.scene, clip_kind,shown);
  }
  lume_jobs_close(clip_jobs);clip_jobs=NULL;clip_jobs_checked=false;
  lume_detail_close(&detail);lume_detail_puzzle_cache_close();detail_foreground_close();
  lume_reader_close(&clue_reader);
  say(NULL);lume_animation_close(&result_timer);lume_animation_close(&meter_transition);lume_animation_close(&lock_progress);
  lume_animation_close(&gramophone_effect);lume_animation_close(&piano_effect);
  lume_animation_close(&puzzle_art);
  close_clip();
  lume_movie_pool_close(&camera_pool);movie=&camera_pool.slots[0].clip;
  (void)lume_persist_request(&game);
  if(return_to_title) {
    active_game=NULL;ending_card_open=cinema_reading=false;credits_loaded=false;
    puzzle_flow.mode=-1;lume_interaction_cancel(&interaction);dialogue_target=-1;navigation_line=after_clue=NULL;
    lume_menu_close(&native_menu);lume_field_close(&ending_field);
    world_scene=movie_id=cached_clue=puzzle_art_mode=-1;
    detail_mode=-1;detail_state=NULL;detail_reference_ready=false;
    memset(&presentation_cache,0,sizeof presentation_cache);visual_epoch++;frame_serial++;
    lume_audio_music(false);goto restart_title;
  }
  lume_props_quit();
  lume_menu_art_close(&native_menu_art);
  lume_audio_quit();lume_read_release(&reading);
  lume_persist_stop();lume_log_io_drain();
  romfsExit();
  lume_gpu_video_close();gfxExit();
  return 0;
}
