# umyo-bootloader

OTA-capable bootloader for uMyo (nRF52832). Enables wireless firmware updates via the Android app over BLE.

**Docs:** https://make.udevices.io
**Discord:** https://discord.com/invite/dEmCPBzv9G

## What this is

A custom BLE bootloader for uMyo v3.1. Once flashed, all future firmware updates can be done wirelessly via the [Android app](https://github.com/ultimaterobotics/umyo-android) — no programmer needed again.

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

## Building from source

```bash
git clone https://github.com/ultimaterobotics/umyo-bootloader.git
cd umyo-bootloader
git submodule update --init --recursive
cd boot_umyo3
make
```

Requires `arm-none-eabi-gcc` (tested with GCC 8.2.1).

## Flash layout

```
0x00000  Bootloader (16KB)
0x04000  Firmware (~48KB)
0x2B000  Calibration storage (wear-leveled, NOT OTA-safe)
0x40000  Device name storage (wear-leveled, OTA-safe)
0x80000  End of flash
```

OTA erase ceiling is hardcapped at 0x40000 — calibration and device name storage are never touched by OTA.

## Related

- [uMyo firmware](https://github.com/ultimaterobotics/uMyo)
- [umyo-android](https://github.com/ultimaterobotics/umyo-android) — OTA update app
- [urf_lib](https://github.com/ultimaterobotics/urf_lib) — shared nRF52 library
