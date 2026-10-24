#ifndef LUME_PERSISTENCE_H
#define LUME_PERSISTENCE_H
#include "game.h"
/* One active snapshot and one pending latest snapshot bound application I/O. */
bool lume_persist_start(void);
void lume_persist_seed(const LumeGame *game);
unsigned lume_persist_request(const LumeGame *game);
/* -1: failure, 0: queued, 1: persisted. Ticket zero never means success. */
int lume_persist_status(unsigned ticket);
void lume_persist_stop(void);
#endif
