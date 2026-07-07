#pragma once
#include <pebble.h>

void list_window_push(void);
void list_window_set_status(const char *status);
void list_window_set_header(const char *header);
// Set the header to "<prefix> HH:MM" from g_list_fetch_time;
// short_prefix is used on narrow displays where the full one truncates
void list_window_show_fetch_time(const char *prefix,
                                 const char *short_prefix);
void list_window_on_list_updated(bool done);
