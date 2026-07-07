var Clay = require('@rebble/clay');
var clayConfig = require('./config');
// autoHandleEvents off: the settings dict goes through our send queue
// so it can't collide with an in-flight list or summary stream.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

// CMD values shared with src/c/comm.c
var CMD = {
  READY: 0,
  GET_LIST: 10,
  GET_SUMMARY: 11,
  LIST_START: 20,
  LIST_ITEM: 21,
  LIST_DONE: 22,
  SUMMARY_CHUNK: 23,
  SUMMARY_DONE: 24,
  ERROR: 25,
  LOC: 26,
};

var MAX_ARTICLES = 20;
var SEARCH_RADIUS_M = 10000; // geosearch API maximum
var SUMMARY_MAX_CHARS = 4000;
var CHUNK_SIZE = 400;
var SEND_RETRIES = 3;
var LOC_SEND_INTERVAL_MS = 3000;
var LOC_MAX_AGE_MS = 30000;
var AUTO_REFRESH_DIST_M = 300;
var AUTO_REFRESH_MIN_INTERVAL_MS = 60000;

// Debug override for the emulator's fixed fake GPS: [lat, lon] or null
var FAKE_LOC = null; // e.g. [64.14660, -21.94260] for emulator testing

var articles = [];
var lastPos = null;
var lastLocSentAt = 0;
var lastFetchLoc = null; // {lat, lon} the current list was fetched from
var lastFetchAt = 0;

// ---- Single ordered send queue -------------------------------------------
// AppMessage allows one message in flight; everything (list items, summary
// chunks, location pushes) goes through one queue so streams never collide.
var queue = [];
var sending = false;

function enqueue(msgs) {
  queue = queue.concat(msgs);
  if (!sending) {
    pump(SEND_RETRIES);
  }
}

function pump(retriesLeft) {
  if (queue.length === 0) {
    sending = false;
    return;
  }
  sending = true;
  var msg = queue[0];
  Pebble.sendAppMessage(
    msg,
    function () {
      queue.shift();
      pump(SEND_RETRIES);
    },
    function () {
      if (retriesLeft > 0) {
        setTimeout(function () {
          pump(retriesLeft - 1);
        }, 250);
      } else {
        console.log('Dropping message after retries: ' + JSON.stringify(msg));
        queue.shift();
        pump(SEND_RETRIES);
      }
    }
  );
}

function sendError(msg) {
  console.log('Error: ' + msg);
  enqueue([{ CMD: CMD.ERROR, ERROR: msg }]);
}

// ---- Location --------------------------------------------------------------

function coordsOf(pos) {
  if (FAKE_LOC) {
    return { latitude: FAKE_LOC[0], longitude: FAKE_LOC[1] };
  }
  return pos.coords;
}

function distMeters(lat1, lon1, lat2, lon2) {
  var dy = (lat2 - lat1) * 111320;
  var dx = (lon2 - lon1) * 111320 * Math.cos(lat1 * Math.PI / 180);
  return Math.sqrt(dx * dx + dy * dy);
}

// Re-fetch the list when we've moved far enough from where it was fetched
function maybeAutoRefresh(coords) {
  if (!lastFetchLoc ||
      Date.now() - lastFetchAt < AUTO_REFRESH_MIN_INTERVAL_MS) {
    return;
  }
  var moved = distMeters(lastFetchLoc.lat, lastFetchLoc.lon,
                         coords.latitude, coords.longitude);
  if (moved > AUTO_REFRESH_DIST_M) {
    console.log('Auto-refresh: moved ' + Math.round(moved) + ' m');
    handleGetList();
  }
}

function startLocationStream() {
  navigator.geolocation.watchPosition(
    function (pos) {
      pos.receivedAt = Date.now();
      lastPos = pos;
      var now = Date.now();
      var c = coordsOf(pos);
      if (now - lastLocSentAt >= LOC_SEND_INTERVAL_MS) {
        lastLocSentAt = now;
        enqueue([{
          CMD: CMD.LOC,
          LOC_LAT: Math.round(c.latitude * 100000),
          LOC_LON: Math.round(c.longitude * 100000),
        }]);
      }
      maybeAutoRefresh(c);
    },
    function (err) {
      console.log('watchPosition error: ' + err.message);
    },
    { enableHighAccuracy: true, maximumAge: 1000, timeout: 30000 }
  );
}

function getLocation(cb) {
  if (lastPos && Date.now() - lastPos.receivedAt < LOC_MAX_AGE_MS) {
    cb(null, coordsOf(lastPos));
    return;
  }
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      pos.receivedAt = Date.now();
      lastPos = pos;
      cb(null, coordsOf(pos));
    },
    function (err) {
      cb(err);
    },
    { timeout: 15000, maximumAge: 60000, enableHighAccuracy: true }
  );
}

// ---- Wikipedia --------------------------------------------------------------

function getLang() {
  try {
    var s = JSON.parse(localStorage.getItem('clay-settings')) || {};
    var custom = String(s.LANG_CUSTOM || '').trim().toLowerCase();
    if (custom) {
      return custom;
    }
    return s.LANG || 'en';
  } catch (e) {
    return 'en';
  }
}

function fetchJSON(url, cb) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    try {
      cb(null, JSON.parse(xhr.responseText));
    } catch (e) {
      console.log('Bad response (' + xhr.status + '): ' +
                  String(xhr.responseText).substring(0, 200));
      cb(e);
    }
  };
  xhr.onerror = function () {
    console.log('XHR error, status ' + xhr.status);
    cb(new Error('request failed'));
  };
  xhr.open('GET', url);
  // Wikipedia rejects requests without a user agent (403). Browsers forbid
  // setting User-Agent, so Wikipedia accepts Api-User-Agent instead; try both.
  var ua = 'nearby-wiki-pebble/1.0 (danielv@dohop.com)';
  try { xhr.setRequestHeader('Api-User-Agent', ua); } catch (e) {}
  try { xhr.setRequestHeader('User-Agent', ua); } catch (e) {}
  xhr.send();
}

function handleGetList() {
  getLocation(function (err, coords) {
    if (err) {
      sendError('No GPS fix');
      return;
    }
    console.log('Location: ' + coords.latitude + ',' + coords.longitude);
    // Set before the fetch completes so a slow request isn't re-triggered
    lastFetchLoc = { lat: coords.latitude, lon: coords.longitude };
    lastFetchAt = Date.now();
    var url =
      'https://' + getLang() + '.wikipedia.org/w/api.php' +
      '?action=query&list=geosearch' +
      '&gscoord=' + coords.latitude + '%7C' + coords.longitude +
      '&gsradius=' + SEARCH_RADIUS_M +
      '&gslimit=' + MAX_ARTICLES +
      '&format=json';
    fetchJSON(url, function (err2, json) {
      if (err2 || !json.query || !json.query.geosearch) {
        sendError('Wikipedia error');
        return;
      }
      articles = json.query.geosearch;
      var msgs = [{
        CMD: CMD.LIST_START,
        COUNT: articles.length,
        LOC_LAT: Math.round(coords.latitude * 100000),
        LOC_LON: Math.round(coords.longitude * 100000),
      }];
      articles.forEach(function (a, i) {
        msgs.push({
          CMD: CMD.LIST_ITEM,
          INDEX: i,
          TITLE: a.title.substring(0, 47),
          LAT: Math.round(a.lat * 100000),
          LON: Math.round(a.lon * 100000),
          DISTANCE: Math.round(a.dist),
        });
      });
      msgs.push({ CMD: CMD.LIST_DONE });
      enqueue(msgs);
    });
  });
}

function handleGetSummary(index) {
  var a = articles[index];
  if (!a) {
    sendError('Unknown article');
    return;
  }
  // TextExtracts with exintro returns the full intro section; the REST
  // page/summary endpoint only returns the first paragraph.
  var url =
    'https://' + getLang() + '.wikipedia.org/w/api.php' +
    '?action=query&prop=extracts&exintro&explaintext&format=json' +
    '&redirects=1&pageids=' + a.pageid;
  fetchJSON(url, function (err, json) {
    var page = json && json.query && json.query.pages &&
               json.query.pages[a.pageid];
    if (err || !page || !page.extract) {
      sendError('No summary');
      return;
    }
    var text = page.extract;
    if (text.length > SUMMARY_MAX_CHARS) {
      text = text.substring(0, SUMMARY_MAX_CHARS - 1) + '…';
    }
    var msgs = [];
    for (var off = 0; off < text.length; off += CHUNK_SIZE) {
      msgs.push({
        CMD: CMD.SUMMARY_CHUNK,
        INDEX: index,
        CHUNK: text.substring(off, off + CHUNK_SIZE),
      });
    }
    msgs.push({ CMD: CMD.SUMMARY_DONE, INDEX: index });
    enqueue(msgs);
  });
}

Pebble.addEventListener('ready', function () {
  console.log('PKJS ready');
  startLocationStream();
  enqueue([{ CMD: CMD.READY }]);
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  switch (p.CMD) {
    case CMD.GET_LIST:
      handleGetList();
      break;
    case CMD.GET_SUMMARY:
      handleGetSummary(p.INDEX);
      break;
  }
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) {
    return;
  }
  var oldLang = getLang();
  // getSettings also persists to localStorage ('clay-settings')
  var settings = clay.getSettings(e.response);
  enqueue([settings]); // delivers UNITS to the watch
  if (getLang() !== oldLang) {
    console.log('Language changed to ' + getLang() + ', refreshing');
    handleGetList();
  }
});
