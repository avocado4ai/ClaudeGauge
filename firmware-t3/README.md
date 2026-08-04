# ClaudeGauge — TTGO T-Display Firmware

Firmware for a TTGO T-Display (ESP32) that acts as a desk gauge showing:

- Claude.ai subscription usage (5-hour and 7-day limits, Opus/Sonnet breakdown, extra spend)
- Home GPU / Ollama status (from a `gpu_server.py` running on the `shuli` server)
- A 7-segment clock

Cycle between pages with the two onboard buttons (configurable — see below).

## How it works

- On boot, the firmware connects to WiFi using credentials stored in NVS (`Preferences`).
- If no WiFi credentials are set, it starts a WiFi access point (`ClaudeGauge-TD`) and a
  web config server at `192.168.4.1` so you can enter WiFi + Claude session key from a phone/laptop.
- Once connected, it polls a proxy server (`proxyUrl`, default
  `https://cloud-proxy-three.vercel.app`) which forwards requests to `claude.ai`'s internal
  usage API using a `X-Session-Key` header. The proxy exists because the ESP32 can't do the
  full claude.ai login flow itself — you paste a `sessionKey` cookie value once via the config page.
- Every `REFRESH_MS` (5 min) it re-fetches usage data; every `OLLAMA_REFRESH_MS` (10 s) it
  polls `shuli` (`192.168.1.118`) directly for GPU stats (`:8765/gpu`) and the running
  Ollama model (`:11434/api/ps`).
- The backlight LED pulses when an Ollama model is actively loaded/running.
- Pages and physical-button actions are data-driven (`/layout.json` and `/buttons.json`
  on LittleFS), rendered at runtime by `layout_engine.cpp` — see **Management page**
  below to edit them live, without reflashing.

## Default pages (cycled with BTN1 / BTN2)

1. **Main** — 5-hour and 7-day usage donuts with countdown timers.
2. **Opus/Sonnet** — per-model usage bars (only meaningful on plans with model-specific limits).
3. **Extra spend** — overage credit usage, if enabled on the account.
4. **Ollama/GPU** — live GPU utilization, VRAM, temperature, and loaded model name from `shuli`.
5. **Clock** — big 7-segment clock, synced via NTP once WiFi is up.
6. **Status** — WiFi RSSI, uptime, last fetch time, IP address.

These are just the built-in defaults — add, remove, reorder, or restyle pages from the
management page's **Pages** tab.

## Management page

Open `http://<device-ip>/` for the on-device management page (served from LittleFS,
`data/index.html` + `app.js` + `style.css`):

- **Setup** — WiFi SSID/password, Claude session key, proxy URL, OTA/admin password.
- **Firmware Update** — web-based OTA upload (`.bin` file), auth-protected.
- **Pages** — add/delete pages, drag to reorder them, enable/disable, and edit each
  page's widgets (gauges, bars, text, clock) on a visual canvas — changes apply live,
  no reflash needed.
- **Buttons** — map BTN1/BTN2 short-press and long-press (600ms+) to actions: next
  page, previous page, jump to a specific page, or toggle the backlight.

Every state-changing route (`/save`, `/update`, `/api/layout`, `/api/buttons`) is
protected with HTTP Basic Auth — username `admin`, password is the OTA/Admin password
(default `claudegauge`). See [OTA.md](OTA.md) for details.

## First-time setup

1. Flash the firmware (see below).
2. On first boot with no saved WiFi, the device starts AP `ClaudeGauge-TD`.
3. Connect to it and open `http://192.168.4.1`.
4. Fill in WiFi SSID/password, the Claude.ai `sessionKey` cookie value, and the proxy URL.
5. Save — the device reboots and connects normally.

To get the session key: open claude.ai in a browser, DevTools → Application/Storage →
Cookies → copy the `sessionKey` value.

## Build / flash

Requires [PlatformIO](https://platformio.org/):

```bash
pio run -e t-display -t upload     # build + flash firmware over USB
pio run -e t-display -t uploadfs   # flash the management page (data/) to LittleFS
pio device monitor -b 115200       # serial log
```

`uploadfs` only needs to be re-run when `data/index.html`/`app.js`/`style.css` change —
firmware updates alone don't touch the LittleFS partition (and won't erase your saved
`/layout.json` / `/buttons.json`).

Once the board is running this firmware, further updates can be pushed wirelessly —
see [OTA.md](OTA.md).

See [HARDWARE.md](HARDWARE.md) for pinout and board details.

## Key files

- `src/main.cpp` — display, WiFi, HTTP fetch, button/backlight loop.
- `src/web_server.cpp` — management web page routes (setup, OTA, layout/buttons API).
- `src/layout_engine.cpp` — JSON-driven page/widget rendering (`/layout.json`).
- `src/button_actions.cpp` — JSON-driven button action mapping (`/buttons.json`).
- `include/ui_helpers.h` — shared low-level TFT drawing primitives.
- `data/` — the management page frontend (uploaded to LittleFS via `uploadfs`).
- `platformio.ini` — board config, TFT_eSPI pin definitions, dependencies.
