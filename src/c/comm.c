#include <pebble.h>
#include "comm.h"
#include "data.h"
#include "list_window.h"
#include "article_window.h"

// CMD values shared with src/pkjs/index.js
enum {
  CMD_READY = 0,
  CMD_GET_LIST = 10,
  CMD_GET_SUMMARY = 11,
  CMD_LIST_START = 20,
  CMD_LIST_ITEM = 21,
  CMD_LIST_DONE = 22,
  CMD_SUMMARY_CHUNK = 23,
  CMD_SUMMARY_DONE = 24,
  CMD_ERROR = 25,
};

static bool s_js_ready = false;
static bool s_list_pending = false;
static int s_summary_index = -1;
static size_t s_summary_len = 0;

static void prv_send(uint8_t cmd, int32_t index, bool has_index) {
  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox unavailable: %d", (int)result);
    return;
  }
  dict_write_uint8(out, MESSAGE_KEY_CMD, cmd);
  if (has_index) {
    dict_write_int32(out, MESSAGE_KEY_INDEX, index);
  }
  app_message_outbox_send();
}

void comm_request_list(void) {
  if (!s_js_ready) {
    // JS sends CMD_READY when it starts; the request fires then
    s_list_pending = true;
    return;
  }
  prv_send(CMD_GET_LIST, 0, false);
}

void comm_request_summary(int index) {
  s_summary_index = index;
  s_summary_len = 0;
  g_summary[0] = '\0';
  prv_send(CMD_GET_SUMMARY, index, true);
}

static void prv_handle_list_item(DictionaryIterator *iter) {
  Tuple *index = dict_find(iter, MESSAGE_KEY_INDEX);
  Tuple *title = dict_find(iter, MESSAGE_KEY_TITLE);
  Tuple *lat = dict_find(iter, MESSAGE_KEY_LAT);
  Tuple *lon = dict_find(iter, MESSAGE_KEY_LON);
  Tuple *distance = dict_find(iter, MESSAGE_KEY_DISTANCE);
  if (!index || !title || !lat || !lon || !distance) {
    return;
  }
  int i = index->value->int32;
  if (i < 0 || i >= MAX_ARTICLES) {
    return;
  }
  Article *a = &g_articles[i];
  strncpy(a->title, title->value->cstring, MAX_TITLE_LEN - 1);
  a->title[MAX_TITLE_LEN - 1] = '\0';
  a->lat = lat->value->int32;
  a->lon = lon->value->int32;
  a->distance_m = distance->value->int32;
  if (i + 1 > g_article_count) {
    g_article_count = i + 1;
  }
}

static void prv_handle_summary_chunk(DictionaryIterator *iter) {
  Tuple *index = dict_find(iter, MESSAGE_KEY_INDEX);
  Tuple *chunk = dict_find(iter, MESSAGE_KEY_CHUNK);
  if (!index || !chunk || index->value->int32 != s_summary_index) {
    return;  // stale chunk for an article we've navigated away from
  }
  size_t len = strlen(chunk->value->cstring);
  size_t space = MAX_SUMMARY_LEN - 1 - s_summary_len;
  if (len > space) {
    len = space;
  }
  memcpy(g_summary + s_summary_len, chunk->value->cstring, len);
  s_summary_len += len;
  g_summary[s_summary_len] = '\0';
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *cmd = dict_find(iter, MESSAGE_KEY_CMD);
  if (!cmd) {
    return;
  }
  switch (cmd->value->int32) {
    case CMD_READY:
      s_js_ready = true;
      if (s_list_pending) {
        s_list_pending = false;
        prv_send(CMD_GET_LIST, 0, false);
      }
      break;
    case CMD_LIST_START:
      g_article_count = 0;
      list_window_set_status("Loading nearby...");
      break;
    case CMD_LIST_ITEM:
      prv_handle_list_item(iter);
      list_window_on_list_updated(false);
      break;
    case CMD_LIST_DONE:
      if (g_article_count == 0) {
        list_window_set_status("Nothing nearby");
      }
      list_window_on_list_updated(true);
      break;
    case CMD_SUMMARY_CHUNK:
      prv_handle_summary_chunk(iter);
      article_window_on_summary(false);
      break;
    case CMD_SUMMARY_DONE:
      article_window_on_summary(true);
      break;
    case CMD_ERROR: {
      Tuple *error = dict_find(iter, MESSAGE_KEY_ERROR);
      const char *msg = error ? error->value->cstring : "Error";
      APP_LOG(APP_LOG_LEVEL_ERROR, "JS error: %s", msg);
      list_window_set_status(msg);
      break;
    }
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason,
                              void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

void comm_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(2048, 256);
}
