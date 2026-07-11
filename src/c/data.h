#pragma once
#include <pebble.h>

#define MAX_ARTICLES 20
// 64 bytes fits ~30 Cyrillic characters (2 bytes each in UTF-8)
#define MAX_TITLE_LEN 64
// Aplite has 24KB total app RAM; halving the summary buffer leaves the
// heap breathing room (JS caps its sends to match)
#ifdef PBL_PLATFORM_APLITE
#define MAX_SUMMARY_LEN 2048
#else
#define MAX_SUMMARY_LEN 4096
#endif

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

// Article text size: 0 = follow the watch-wide content size setting,
// 1..4 = small/medium/large/extra-large override
void data_set_text_size(int pref);
void data_load_text_size(void);

// Whether the current content needs Cyrillic glyphs (detected phone-side
// from article titles); switches titles and body to bundled fonts, since
// the system fonts only cover Latin
extern bool g_cyrillic;
void data_set_cyrillic(bool cyrillic);
void data_load_cyrillic(void);

// Fonts honoring the size preference and the Cyrillic flag
GFont data_body_font(void);
GFont data_title_font(void);

// Read-article tracking: a persisted rolling set of title hashes
void data_mark_read(const char *title);
bool data_is_read(const char *title);
void data_load_read_set(void);

// Live distance (meters) / bearing (Pebble trig angle, clockwise from north)
// from the current location to an article. Distance falls back to the
// fetch-time value when no GPS fix has arrived yet.
int32_t data_distance_to(const Article *a);
int32_t data_bearing_to(const Article *a);
