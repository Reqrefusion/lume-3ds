#ifndef LUME_JOBS_STATE_H
#define LUME_JOBS_STATE_H
/* One application-owned auxiliary CPU budget, shared across translation
   units. Submission never waits for this budget; the caller runs locally
   when another producer owns it. This also prevents nested worker waits. */
#include <stdbool.h>
#include <stddef.h>
bool lume_worker_claim(void);
void lume_worker_release(void);
void lume_worker_counts(unsigned *submitted,unsigned *busy);
bool lume_runtime_flag(const char *name);
void lume_runtime_set_flags_directory(const char *directory);
void lume_runtime_set_asset_directory(const char *directory);
bool lume_runtime_asset_path(char *out,size_t bytes,const char *relative);
#endif
