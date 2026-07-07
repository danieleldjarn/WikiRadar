#pragma once
#include <pebble.h>

#define MAX_ARTICLES 20
#define MAX_TITLE_LEN 48
#define MAX_SUMMARY_LEN 4096

typedef struct {
  char title[MAX_TITLE_LEN];
  int32_t lat;         // degrees * 100000
  int32_t lon;         // degrees * 100000
  int32_t distance_m;  // meters from fetch location
} Article;

extern Article g_articles[MAX_ARTICLES];
extern int g_article_count;
extern char g_summary[MAX_SUMMARY_LEN];
extern bool g_units_imperial;

// Most recent phone GPS fix, degrees * 100000
extern int32_t g_cur_lat;
extern int32_t g_cur_lon;
extern bool g_has_location;

// Watch time when the current list arrived (or cache fetch time)
extern time_t g_list_fetch_time;

void data_format_distance(int32_t meters, char *buf, size_t buf_len);

// Set (and persist) / load the units preference
void data_set_units(bool imperial);
void data_load_units(void);

// Live distance (meters) / bearing (Pebble trig angle, clockwise from north)
// from the current location to an article. Distance falls back to the
// fetch-time value when no GPS fix has arrived yet.
int32_t data_distance_to(const Article *a);
int32_t data_bearing_to(const Article *a);
