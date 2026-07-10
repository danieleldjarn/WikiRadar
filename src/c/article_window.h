#pragma once
#include <pebble.h>

void article_window_push(int article_index);
void article_window_on_summary(bool done);
void article_window_on_location(void);
void article_window_on_text_size(void);
