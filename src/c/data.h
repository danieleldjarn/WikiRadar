#pragma once
#include <pebble.h>

#define MAX_ARTICLES 20
#define MAX_TITLE_LEN 48
#define MAX_SUMMARY_LEN 2048

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

void data_format_distance(int32_t meters, char *buf, size_t buf_len);
