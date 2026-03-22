# umyo-bootloader

OTA-capable bootloader for uMyo (nRF52832). Enables wireless firmware updates via the Android app over BLE.

**Docs:** https://make.udevices.io
**Discord:** https://discord.com/invite/dEmCPBzv9G

## What this is

A custom BLE OTA bootloader for uMyo v3.1, derived from the public `uECG_bootloader` but meaningfully different: board config is centralized in `boot_config.h`, button logic is debounced and configurable, BLE/RF mode selection happens at runtime, and the OTA entry sequence requires an explicit extra step to enter BLE mode (the default boot mode is base/radio, not BLE).

Once the bootloader is flashed, all future firmware updates can be done wirelessly via the [Android app](https://github.com/ultimaterobotics/umyo-android) — no programmer needed again.

## Repository structure

```
boot_umyo3/     uMyo v3.1 bootloader variant
  main.c
  Makefile
  boot_config.h
  uboot_ble.ld
  upload.sh
urf_lib/        submodule — nRF52 BLE stack
CLAUDE.md       developer notes
```

## Flashing the bootloader

Requires a one-time wired flash via SWD programmer (ST-Link v2 clone, Raspberry Pi Pico with picoprobe, or any CMSIS-DAP probe).

Pre-built hex files are available in [Releases](https://github.com/ultimaterobotics/umyo-bootloader/releases).

See the [SWD flashing guide](https://make.udevices.io/guides/firmware-flash-swd) for step-by-step instructions including programmer options and openocd commands.

After flashing the bootloader once, all future firmware updates are wireless via the Android app.

## BLE OTA entry sequence

The bootloader does not enter BLE mode automatically — it starts in base/radio mode and requires an explicit step to switch. The full sequence:

1. **Power off** the device completely.
2. **Press and hold** the button. The LED turns solid blue instead of running the normal startup sequence — you are now in bootloader mode.
3. **Keep holding** for 5+ seconds until the LED switches to a slow blue blink. The device is now in bootloader mode.
4. **Short press once** to switch to BLE mode. There is no LED change at this step — that is expected.
5. The device now advertises as **`uECG boot`** (the advertising name is fixed at `uECG boot` regardless of configured GAP name — this is expected, not a bug).
6. Open the Android app, scan for `uECG boot`, connect, and start the OTA update.

## OTA transfer

- Protocol: stop-and-wait — each 16-byte packet is acknowledged before the next is sent.
- Transfer time: ~47.8 kB firmware takes approximately **4–5 minutes**. This is normal, not a hang.
- A 15-minute timeout (`CFG_BOOTLOADING_TIMEOUT`) acts as a failsafe only; it does not reflect the expected transfer time.

Tested on:
- Samsung A5 2017
- Pixel 4a 5G

iOS is completely untested. Do not assume it works.

## Building from source

```bash
git clone https://github.com/ultimaterobotics/umyo-bootloader.git
cd umyo-bootloader
git submodule update --init --recursive
cd boot_umyo3
make clean
make
```

Requires `arm-none-eabi-gcc` (tested with GCC 8.2.1). `SDK_ROOT` resolves to `urf_lib/nrf_usdk52` — the submodule must be initialized before building. Always run `make clean` before `make`, especially after any changes to `urf_lib`.

## Flash layout

```
0x00000  Bootloader (16KB)
0x04000  Firmware (~48KB)
0x2B000  Calibration storage (wear-leveled, NOT OTA-safe)
0x40000  Device name storage (wear-leveled, OTA-safe)
0x80000  End of flash
```

OTA erase ceiling is hardcapped at 0x40000 — calibration and device name storage are never touched by OTA.

## Known limitations

- **No full-image verification.** Verification is packet-level only — dropped or reordered packets are handled, but there is no end-to-end CRC or hash check of the completed image.
- **iOS untested.** No testing on iPhone or any iOS device has been done.

## Critical: do not revert BLE_FORCE_SAME_EVENT_RESPONSE

`BLE_FORCE_SAME_EVENT_RESPONSE` must remain `0` in `urf_lib`. If set to `1`, the bootloader disconnects immediately after BLE connect on Android — making OTA impossible. This was the root cause of a connect-stage hang that took significant effort to diagnose. Do not revert this setting even if it looks like cleanup.

## Related

- [uMyo firmware](https://github.com/ultimaterobotics/uMyo)
- [umyo-android](https://github.com/ultimaterobotics/umyo-android) — OTA update app
- [urf_lib](https://github.com/ultimaterobotics/urf_lib) — shared nRF52 library
