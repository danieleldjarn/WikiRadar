// CMD values shared with the watch side
var CMD_PING = 0;
var CMD_PONG = 1;

Pebble.addEventListener('ready', function () {
  console.log('PKJS ready, pinging watch');
  Pebble.sendAppMessage(
    { CMD: CMD_PING },
    function () {
      console.log('Ping delivered to watch');
    },
    function (e) {
      console.log('Ping failed: ' + JSON.stringify(e));
    }
  );
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload.CMD === CMD_PONG) {
    console.log('Pong received from watch — link is up');
  }
});
