"""
patch_tft_espi_c6.py  —  Pre-build extra_script for the [env:c6amoled] environment.

TFT_eSPI 2.5.43 does not recognise CONFIG_IDF_TARGET_ESP32C6, so on
ESP32-C6 it falls through to the old Xtensa processor path
(TFT_eSPI_ESP32.h / TFT_eSPI_ESP32.c) which uses registers and constants
that don't exist on C6 (VSPI, SPI_MOSI_DLEN_REG, GPIO.out_w1tc.val ...).

We can't add -DCONFIG_IDF_TARGET_ESP32C3 as a global build flag because
esp32-hal-cpu.c uses that macro to pick the C3 ROM header, which in turn
tries to include soc/rtc_cntl_reg.h — a file absent on C6.

Instead, this script patches the three TFT_eSPI source files in the
local libdeps cache to add ESP32-C6 alongside ESP32-C3 wherever
TFT_eSPI's own processor-selection logic looks for it.  The patches are
idempotent (applied only once) and survive pio run --clean.

This is a pre: extra_script, so its top-level code executes before the
build starts — no AddPreAction wrapper is needed.
"""

# PlatformIO injects `env` via SCons when it loads extra_scripts.
Import("env")   # noqa: F821
import os

MARKER = "/* patched-for-esp32c6 */"

libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")   # noqa: F821
pioenv      = env["PIOENV"]                         # noqa: F821 — e.g. "c6amoled"
lib_dir     = os.path.join(libdeps_dir, pioenv, "TFT_eSPI")

PATCHES = {
    # ── TFT_eSPI.h: processor header selection ──────────────────────────────
    os.path.join(lib_dir, "TFT_eSPI.h"): [
        (
            '#elif defined(CONFIG_IDF_TARGET_ESP32C3)\n'
            '  #include "Processors/TFT_eSPI_ESP32_C3.h"',

            '#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)\n'
            '  #include "Processors/TFT_eSPI_ESP32_C3.h"',
        ),
    ],
    # ── TFT_eSPI.cpp: processor implementation selection ────────────────────
    os.path.join(lib_dir, "TFT_eSPI.cpp"): [
        (
            '#elif defined(CONFIG_IDF_TARGET_ESP32C3)\n'
            '    #include "Processors/TFT_eSPI_ESP32_C3.c"',

            '#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)\n'
            '    #include "Processors/TFT_eSPI_ESP32_C3.c"',
        ),
    ],
    # ── TFT_eSPI_ESP32_C3.h: two patches needed ─────────────────────────────
    os.path.join(lib_dir, "Processors", "TFT_eSPI_ESP32_C3.h"): [
        # 1. Prevent the empty "#define CONFIG_IDF_TARGET_ESP32" on C6.
        #    Without this guard the macro is defined with no value, which
        #    makes "#if CONFIG_IDF_TARGET_ESP32" in sha_types.h expand to
        #    "#if " (no expression) → compile error.  Also define as 1 so
        #    that "#ifdef" usages see a truthy value on plain ESP32.
        (
            '#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32)\n'
            '  #define CONFIG_IDF_TARGET_ESP32\n'
            '#endif',

            '#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2) \\\n'
            ' && !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C6)\n'
            '  #define CONFIG_IDF_TARGET_ESP32 1\n'
            '#endif',
        ),
        # 2. Extend the SPI_MOSI_DLEN_REG fix to C6 (renamed on RISC-V).
        (
            '#if CONFIG_IDF_TARGET_ESP32C3\n'
            '  // Fix ESP32C3 IDF bug for missing definition',

            '#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6\n'
            '  // Fix ESP32C3/C6 IDF bug for missing definition',
        ),
    ],
}


def apply_patches():
    if not os.path.isdir(lib_dir):
        print(
            "[patch_tft_espi_c6] TFT_eSPI not found in libdeps yet — "
            "will be patched after first library install."
        )
        return

    for filepath, replacements in PATCHES.items():
        if not os.path.isfile(filepath):
            print(f"[patch_tft_espi_c6] WARNING – file not found: {filepath}")
            continue

        with open(filepath, "r", encoding="utf-8") as fh:
            text = fh.read()

        if MARKER in text:
            continue  # already patched

        changed = False
        for old, new in replacements:
            if old in text:
                text = text.replace(old, new, 1)
                changed = True
            else:
                print(
                    f"[patch_tft_espi_c6] WARNING – pattern not found in "
                    f"{os.path.basename(filepath)}; library version may differ."
                )

        if changed:
            text = MARKER + "\n" + text
            with open(filepath, "w", encoding="utf-8") as fh:
                fh.write(text)
            print(f"[patch_tft_espi_c6] Patched {os.path.basename(filepath)}")


apply_patches()
