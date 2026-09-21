# Third-party notices — Rackglow

This project is MIT licensed (see `LICENSE`). Listed below is the third-party software
distributed with, or linked into, the firmware, each of which remains under its own
licence.

Versions and licences were read from the **resolved** packages in `.pio/libdeps/` and
`web_src/node_modules/`, not from memory. `lib_deps` pins its direct dependencies
exactly, but AsyncTCP and ESPAsyncWebServer arrive through Preflight's own version
ranges and can move — re-check this table before any release.

## Linked into the firmware

| Dependency | Version | Licence |
|---|---|---|
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.4.3 | MIT |
| [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) | 3.5.0 | **LGPL-3.0** |
| [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) | 3.12.1 | **LGPL-3.0** |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | 1.2.28 | MIT **and** FreeBSD (BSD-2-Clause) — see note |
| [LVGL](https://github.com/lvgl/lvgl) | 9.5.0 | MIT |
| Montserrat (LVGL built-in fonts) | with LVGL 9.5.0 | **OFL-1.1** — see note |
| [Preflight](https://github.com/diydoohickeys/Preflight-Arduino) | 1.0.0 | MIT |
| `lib/led_strip` (vendored) | Espressif, 2022–2024 | Apache-2.0 |
| [arduino-esp32](https://github.com/espressif/arduino-esp32) (via the pioarduino platform) | per `platformio.ini` | LGPL-2.1, over ESP-IDF (Apache-2.0) |

## Bundled in the web UI

| Dependency | Version | Licence |
|---|---|---|
| [Vue](https://github.com/vuejs/core) | 3.5.43 | MIT |

The Vue runtime is compiled into the single `app.js` that `build_web.py` embeds in the
firmware image, so it ships in the binary rather than being fetched at runtime. Vite and
`@vitejs/plugin-vue` are build-time only and are not distributed.

## Notes

**LovyanGFX** ships one `license.txt` covering several origins: the library's own code
under the FreeBSD (BSD-2-Clause) licence, plus retained headers from Adafruit's
`Adafruit_ILI9341` (MIT) and Bodmer's TFT_eSPI (FreeBSD). All of it must travel with any
redistribution — see that file in the package.

**Montserrat** is embedded as LVGL bitmap fonts rather than as font files.
`include/lv_conf.h` enables sizes 10, 12, 14, 18, 22 and 32, so the SIL Open Font Licence
1.1 applies to the firmware image. It is a permissive licence; its only real condition
here is that the font's name not be used to promote derivative fonts.

**LGPL-3.0 (AsyncTCP, ESPAsyncWebServer)** and **LGPL-2.1 (arduino-esp32)** are the only
copyleft licences in the set. Both are satisfied by distributing this firmware's own
source — which is public and MIT — alongside the unmodified upstream sources, and neither
library is modified here.

**The MSGEQ7 driver** (`include/audio/Msgeq7.h`, `src/audio/Msgeq7.cpp`) is original work,
written from the MSGEQ7 datasheet's timing table. It replaced a third-party library that
stated no licence anywhere, and shares no code with it.
