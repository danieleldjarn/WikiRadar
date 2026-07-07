#pragma once
#include <pebble.h>

void list_window_push(void);
void list_window_set_status(const char *status);
void list_window_on_list_updated(bool done);
