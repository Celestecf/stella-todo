# Stella's Day

A kid-friendly daily to-do list on an ESP32 touchscreen, managed by parents
from a small web portal.

## Layout

| Folder | What it is |
|---|---|
| `stella_todo/` | The display firmware (Arduino sketch). Three pages: Before School, After School, Bedtime. |
| `touch_calibrate/` | Run once to calibrate the touch panel; paste its output into `stella_todo/touch_config.h`. |
| `display_hello/`, `sketch_sep15b/` | The first "is it alive?" test sketches. |
| `portal/` | Parent portal: static site + Netlify Functions + Netlify Blobs. |
| `design/` | Mockup and the script that slices icons out of it into `stella_todo/icons.h`. |
| `TFT_eSPI_User_Setup.h` | Copy of the TFT_eSPI `User_Setup.h` for this board (goes in `Arduino/libraries/TFT_eSPI/`). |

## Hardware

Inland / ESP32-2432S028 "Cheap Yellow Display": ESP32-WROOM-32E, 2.8" ILI9341
240x320 display, XPT2046 resistive touch. In Arduino IDE pick **ESP32 Dev Module**.

Libraries: `TFT_eSPI` (Bodmer), `XPT2046_Touchscreen` (Paul Stoffregen).

## Time-of-day behaviour

The device syncs its clock from the internet (NTP) at boot. Settings are at the
top of `stella_todo/stella_todo.ino`:

| Setting | Default | What it does |
|---|---|---|
| `TZ_INFO` | `EST5EDT,M3.2.0,M11.1.0` (New York) | must match `TIMEZONE` on the server |
| `AFTER_SCHOOL_MIN` | 2:30 PM | switches to the After School page |
| `BEDTIME_MIN` | 7:00 PM | switches to the Bedtime page |
| `NIGHT_START_MIN` / `NIGHT_END_MIN` | 8:30 PM - 6:00 AM | backlight off; a touch wakes it for `WAKE_MS` |

The arrows still work at any time; the schedule only changes the page when a
boundary is crossed.

## Portal

See [`portal/README.md`](portal/README.md).
