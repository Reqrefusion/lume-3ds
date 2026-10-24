#ifndef LUME_DOCUMENT_READING_H
#define LUME_DOCUMENT_READING_H
#include "engine/features/reading/bubble.h"
#include "clue_views.h"
typedef struct {bool active,top;int owner;} LumeDocumentReading;
static inline const char *lume_document_label(const LumeGame *g,char *name,size_t size){
 if(g->clue!=0&&g->clue!=1&&g->clue!=3&&g->clue!=4)return NULL;
 char variant[32];const char *asset=lume_clue_asset(g,variant,sizeof variant);
 snprintf(name,size,"doc_%s",asset);return name;
}
static inline void lume_document_close(LumeDocumentReading *d,LumeReading *r){
 if(d->active)lume_read_close(r);
 d->active=false;d->owner=-1;
}
static inline bool lume_document_sync(LumeDocumentReading *d,LumeReading *r,const LumeGame *g){
 if(d->active&&(!r->active||d->owner!=lume_clue_cache_key(g))){lume_document_close(d,r);return true;}
 return false;
}
static inline bool lume_document_open(LumeDocumentReading *d,LumeReading *r,const LumeGame *g){
 char label[64];if(!lume_document_label(g,label,sizeof label)||!lume_read_open(r,label))return false;
 d->active=true;d->owner=lume_clue_cache_key(g);return true;
}
/* Y closes just this view at the end. The photographed document remains open.
   Routing changes keep the current text offset when the new width repaginates. */
static inline bool lume_document_next(LumeDocumentReading *d,LumeReading *r){
 if(!d->active||!lume_read_step(r,false))return false;
 if(r->finished)lume_document_close(d,r);
 return true;
}
static inline bool lume_document_route(LumeDocumentReading *d,LumeReading *r,bool top){
 if(!d->active||d->top==top)return false;
 d->top=top;r->presented=false;r->serial++;return true;
}
#endif
