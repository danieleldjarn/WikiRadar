#include "cache.h"
#include "data.h"

enum {
  PKEY_VERSION = 1,
  PKEY_COUNT = 2,
  PKEY_FETCH_TIME = 3,
  PKEY_ARTICLE_BASE = 10,  // ..29, one key per article
  PKEY_SUM_TITLE = 40,
  PKEY_SUM_LEN = 41,
  PKEY_SUM_BASE = 42,  // ..50, summary text in 250-byte chunks
};

#define CACHE_VERSION 1
#define SUM_CHUNK 250

void cache_save_list(void) {
  persist_write_int(PKEY_VERSION, CACHE_VERSION);
  persist_write_int(PKEY_COUNT, g_article_count);
  persist_write_int(PKEY_FETCH_TIME, (int32_t)g_list_fetch_time);
  for (int i = 0; i < g_article_count; i++) {
    persist_write_data(PKEY_ARTICLE_BASE + i, &g_articles[i],
                       sizeof(Article));
  }
}

bool cache_load_list(void) {
  if (persist_read_int(PKEY_VERSION) != CACHE_VERSION) {
    return false;
  }
  int count = persist_read_int(PKEY_COUNT);
  if (count <= 0 || count > MAX_ARTICLES) {
    return false;
  }
  for (int i = 0; i < count; i++) {
    if (persist_read_data(PKEY_ARTICLE_BASE + i, &g_articles[i],
                          sizeof(Article)) != sizeof(Article)) {
      g_article_count = 0;
      return false;
    }
  }
  g_article_count = count;
  g_list_fetch_time = (time_t)persist_read_int(PKEY_FETCH_TIME);
  return true;
}

void cache_save_summary(const char *title) {
  int len = strlen(g_summary);
  if (len == 0) {
    return;
  }
  persist_write_string(PKEY_SUM_TITLE, title);
  persist_write_int(PKEY_SUM_LEN, len);
  for (int off = 0; off < len; off += SUM_CHUNK) {
    int n = len - off < SUM_CHUNK ? len - off : SUM_CHUNK;
    persist_write_data(PKEY_SUM_BASE + off / SUM_CHUNK, g_summary + off, n);
  }
}

bool cache_load_summary(const char *title) {
  char cached_title[MAX_TITLE_LEN];
  if (persist_read_string(PKEY_SUM_TITLE, cached_title,
                          sizeof(cached_title)) <= 0 ||
      strcmp(cached_title, title) != 0) {
    return false;
  }
  int len = persist_read_int(PKEY_SUM_LEN);
  if (len <= 0 || len >= MAX_SUMMARY_LEN) {
    return false;
  }
  for (int off = 0; off < len; off += SUM_CHUNK) {
    int n = len - off < SUM_CHUNK ? len - off : SUM_CHUNK;
    if (persist_read_data(PKEY_SUM_BASE + off / SUM_CHUNK, g_summary + off,
                          n) != n) {
      g_summary[0] = '\0';
      return false;
    }
  }
  g_summary[len] = '\0';
  return true;
}
