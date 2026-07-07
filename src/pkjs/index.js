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
};

var MAX_ARTICLES = 20;
var SEARCH_RADIUS_M = 10000; // geosearch API maximum
var SUMMARY_MAX_CHARS = 2000;
var CHUNK_SIZE = 400;
var SEND_RETRIES = 3;

var articles = [];

function getLang() {
  return localStorage.getItem('lang') || 'en';
}

function sendError(msg) {
  console.log('Error: ' + msg);
  Pebble.sendAppMessage({ CMD: CMD.ERROR, ERROR: msg });
}

// Send messages strictly one at a time; AppMessage allows a single
// message in flight, and ordering matters for list items and chunks.
function sendQueue(queue, retriesLeft) {
  if (queue.length === 0) return;
  if (retriesLeft === undefined) retriesLeft = SEND_RETRIES;
  var msg = queue[0];
  Pebble.sendAppMessage(
    msg,
    function () {
      queue.shift();
      sendQueue(queue);
    },
    function () {
      if (retriesLeft > 0) {
        setTimeout(function () {
          sendQueue(queue, retriesLeft - 1);
        }, 250);
      } else {
        console.log('Dropping message after retries: ' + JSON.stringify(msg));
        queue.shift();
        sendQueue(queue);
      }
    }
  );
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

function getLocation(cb) {
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      cb(null, pos.coords);
    },
    function (err) {
      cb(err);
    },
    { timeout: 15000, maximumAge: 60000, enableHighAccuracy: true }
  );
}

function handleGetList() {
  getLocation(function (err, coords) {
    if (err) {
      sendError('No GPS fix');
      return;
    }
    console.log('Location: ' + coords.latitude + ',' + coords.longitude);
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
      var msgs = [{ CMD: CMD.LIST_START, COUNT: articles.length }];
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
      sendQueue(msgs);
    });
  });
}

function handleGetSummary(index) {
  var a = articles[index];
  if (!a) {
    sendError('Unknown article');
    return;
  }
  var url =
    'https://' + getLang() + '.wikipedia.org/api/rest_v1/page/summary/' +
    encodeURIComponent(a.title.replace(/ /g, '_'));
  fetchJSON(url, function (err, json) {
    if (err || !json.extract) {
      sendError('No summary');
      return;
    }
    var text = json.extract;
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
    sendQueue(msgs);
  });
}

Pebble.addEventListener('ready', function () {
  console.log('PKJS ready');
  sendQueue([{ CMD: CMD.READY }]);
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
