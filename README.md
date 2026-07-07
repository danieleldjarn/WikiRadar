# nearby-wiki
A PebbleOS app that shows you nearby Wikipedia entries based on your location.

Targets the Pebble Time 2 (emery). The watchapp is written in C; the phone-side
companion (PebbleKit JS) handles GPS and Wikipedia API calls.

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
