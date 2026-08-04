# OTA (Over-the-Air) Updates

Once the board has been flashed once over USB with OTA support, all further updates can
be pushed wirelessly over the local network — no cable needed.

## How it works

- The firmware runs [`ArduinoOTA`](https://github.com/espressif/arduino-esp32/tree/master/libraries/ArduinoOTA),
  started in `setupOTA()` (`src/main.cpp`) right after WiFi connects (and re-armed after
  any WiFi reconnect).
- It advertises itself over mDNS as **`claudegauge-td.local`**, port 3232.
- Updates are password-protected. Default password: `claudegauge` — change it via the
  "OTA Password" field on the web config page (`http://<device-ip>/`); it's stored in NVS
  under `ota_pass`.
- A new firmware image is written to the *inactive* app partition, then the bootloader
  switches to it and reboots. The old image stays in place as a fallback slot.

## Method 1 — PlatformIO wireless upload (recommended for dev)

```bash
pio run -e t-display-ota -t upload
```

This targets `claudegauge-td.local` with the auth flag baked into `platformio.ini`
(`upload_flags = --auth=claudegauge`). If you changed the OTA password on the device,
update that line to match.

If mDNS resolution fails on your network, replace `upload_port = claudegauge-td.local`
in `platformio.ini` with the device's IP address directly (visible on the Status screen
or via `pio device monitor` on boot).

## Method 2 — Web upload (no PlatformIO needed)

1. Open `http://<device-ip>/` in a browser (phone or laptop, same network), go to the
   **Firmware Update** tab.
2. The browser will prompt for credentials — username `admin`, password is the
   device's OTA/Admin password (default `claudegauge`, set on the **Setup** tab).
3. Choose the `.bin` file (`.pio/build/t-display/firmware.bin` after a `pio run -e t-display`)
   and click **Upload & Flash**.
4. The device reboots automatically once the upload finishes.

This path is now protected with HTTP Basic Auth (see `src/web_server.cpp`,
`mgmtServer::begin`) using the same password as `ArduinoOTA` — anyone without the
password cannot flash over the web path anymore. Still keep the device on a trusted
network, since Basic Auth over plain HTTP is only a deterrent against casual access,
not a hardened defense against an on-path attacker.

## Why the partition scheme matters

OTA needs **two app partition slots** (`ota_0` / `ota_1`) so the new image can be
written while the old one still runs, then swapped. `platformio.ini` uses
`board_build.partitions = min_spiffs.csv`, which gives ~1.9 MB per slot (current
firmware is ~1.1 MB, so there's headroom to grow).

Do **not** switch back to `huge_app.csv` — it defines a single oversized app partition
with no second OTA slot. `Update.begin()` will still "succeed" but the erase step
writes into the wrong flash region and the device panics mid-update
(`esp_flash_erase_region` abort). If you ever change `board_build.partitions`, re-flash
once over USB — a partition table change can't be applied via OTA.

## Troubleshooting

- **`Sending invitation... Authenticating...OK` then upload fails at 0%** — usually a
  stale/incompatible partition table on the device (see above), or the device is busy
  in a long blocking call (e.g. mid `fetchData()` HTTPS request) when the OTA session
  starts. Retry — `ArduinoOTA.handle()` runs every loop iteration once WiFi is up.
- **mDNS name doesn't resolve** — use the device's IP directly instead of
  `claudegauge-td.local`.
- **Upload hangs at "Waiting for device..."** — check the device is on the same WiFi
  network/subnet as your machine (not the `ClaudeGauge-TD` AP setup mode).
