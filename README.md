# WikiRadar
A PebbleOS app that shows you nearby Wikipedia entries based on your location.

Supports all PebbleOS watches: the originals (aplite), Pebble Time (basalt),
Time Round (chalk), Pebble 2 (diorite), and the Core Devices Pebble Time 2
(emery), Pebble 2 Duo (flint), and Pebble Round 2 (gabbro). The watchapp is
written in C; the phone-side companion (PebbleKit JS) handles GPS and
Wikipedia API calls.

## Building & running

```sh
pebble build                          # build the watchapp
pebble install --emulator emery       # run in the emery emulator
pebble install --phone <ip>           # sideload to a paired phone
```

## Project layout

```
src/c/           C source for the watchapp (UI, compass, formatting)
src/pkjs/        PebbleKit JS — runs on the phone (GPS, Wikipedia fetch)
package.json     Project metadata (UUID, platforms, message keys)
wscript          Build rules
```

## Install

Get it from the [Pebble Appstore](https://apps.repebble.com/61288080085549ccb302c108),
or sideload the `.pbw` from the [latest release](https://github.com/danieleldjarn/WikiRadar/releases).

## License

[MIT](LICENSE). The bundled DejaVu fonts in `resources/fonts/` are
distributed under their own license (see
[`resources/fonts/DEJAVU-LICENSE`](resources/fonts/DEJAVU-LICENSE)).
