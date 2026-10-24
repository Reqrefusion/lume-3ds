#ifndef LUME_STORAGE_H
#define LUME_STORAGE_H
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#ifndef LUME_DATA_DIRECTORY
#define LUME_DATA_DIRECTORY "sdmc:/3ds/lume"
#endif
#define LUME_DATA_FILE(relative) LUME_DATA_DIRECTORY "/" relative
static inline void lume_storage_init(void) {
#ifdef __3DS__
 mkdir("sdmc:/3ds",0777);mkdir(LUME_DATA_DIRECTORY,0777);
#endif
}
static inline bool lume_storage_content_ready(void) {
 const char *paths[]={LUME_DATA_FILE("map_ink.bin"),LUME_DATA_FILE("lang/languages.txt"),LUME_DATA_FILE("media_0175.lvs")};
 for(unsigned i=0;i<sizeof paths/sizeof paths[0];i++){FILE *f=fopen(paths[i],"rb");if(!f)return false;fclose(f);}
 return true;
}
#endif