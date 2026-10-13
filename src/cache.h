#ifndef XLUNCH_CACHE_H
#define XLUNCH_CACHE_H
#include <stdint.h>

typedef struct cache_io { FILE *file; unsigned char *position,*end; int valid; uint32_t hash; } cache_io_t;
extern int rebuild_cache;
uint64_t cache_hash(uint64_t hash, const char *value);
uint64_t ui_cache_config(uint64_t hash);
void cache_bytes(cache_io_t *io, void *data, size_t size);
uint32_t cache_number(cache_io_t *io, uint32_t value);
char *cache_text(cache_io_t *io, const char *value);
void ui_icons_cache(cache_io_t *io);
void ui_icons_reset(void);
void ui_warm_icons(void);
int cache_load(int refresh);
int cache_rebuild(void);
void cache_start(void);
void cache_update(void);
void cache_cancel(void);
void focus_catalog_changed(node_t *catalog);
#endif
