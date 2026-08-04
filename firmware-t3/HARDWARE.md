# Hardware — TTGO T-Display

This firmware targets the original **TTGO T-Display** (ESP32, not the S3 variant),
identified by `board = esp32dev` + manual `ST7789_DRIVER` pin setup in `platformio.ini`
rather than a T-Display-S3-specific board definition.

## Board

- **MCU**: ESP32 (dual-core, WiFi + BT), 4MB flash (`huge_app.csv` partition scheme —
  large app, small SPIFFS).
- **Display**: 1.14" ST7789 TFT, 135×240 native resolution, driven landscape via
  `setRotation(1)` → 240×135 usable in firmware.
- **USB-serial**: CP2104/CH9102-class UART bridge (shows up as `/dev/cu.usbserial-*` or
  `/dev/cu.SLAB_USBtoUART` on macOS).

## Pinout (from `platformio.ini` build_flags)

| Function      | GPIO | Notes                          |
|---------------|------|---------------------------------|
| TFT MOSI      | 19   | SPI data                        |
| TFT SCLK      | 18   | SPI clock                       |
| TFT CS        | 5    | Chip select                     |
| TFT DC        | 16   | Data/command                    |
| TFT RST       | 23   | Reset                           |
| TFT Backlight | 4    | PWM-driven via `ledcAttachPin`  |
| Button 1      | 35   | Input only, active HIGH         |
| Button 2      | 0    | Input pullup, active LOW (also boot-mode strap pin) |

SPI frequency: 40 MHz write / 6 MHz read (`SPI_FREQUENCY`, `SPI_READ_FREQUENCY`).

## Buttons

- **BTN_1 (GPIO35)**: input-only pin (no internal pullup available on ESP32), reads HIGH
  when pressed on this board's wiring. Advances the screen.
- **BTN_2 (GPIO0)**: also the boot-strap pin (must be LOW at reset to enter flash mode),
  used here with `INPUT_PULLUP` and read as pressed when LOW. Also advances the screen
  in current firmware (both buttons do the same action).

Both buttons are debounced with a 300 ms `lastBtnPress` guard in `loop()`.

## Backlight

Backlight is on GPIO4, driven via the ESP32 LEDC PWM peripheral (channel 0, 5 kHz, 8-bit
duty). Firmware uses this for a breathing effect: full brightness normally, pulsing
between low and full duty when an Ollama model is actively loaded on `shuli`.

## Power / flashing

- Flash and serial monitor over the onboard USB-C/micro-USB port at 921600 baud (upload)
  / 115200 baud (monitor).
- No manual BOOT/RESET button sequence needed under normal conditions — `esptool` resets
  the board automatically via RTS/DTR.
