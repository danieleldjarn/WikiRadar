#pragma once
#include <pebble.h>

void cache_save_list(void);
bool cache_load_list(void);

// One summary is cached: the most recently read article, keyed by title.
void cache_save_summary(const char *title);
bool cache_load_summary(const char *title);
