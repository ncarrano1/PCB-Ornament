# PCB Ornament firmware

Firmware for a round PCB Christmas ornament: an STM32F103RBT6 driving a ring of
40 green LEDs, powered by one CR2477 coin cell. The goal is a smooth "traveling
wave" of brightness around the ring, a mode button, and an auto-sleep timer.

The hardware is built and cannot change. All constraints below come from the
real board, so treat them as fixed.

## Build and flash

- PlatformIO, `framework = stm32cube` (ST HAL). One environment per program:
  - `pio run -e ornament -t upload` (**final firmware, default env**: 20 modes,
    battery gauge, sleep, mode memory)
  - `pio run -e blink_one -t upload` (blinks D1)
  - `pio run -e ring_chase -t upload` (one LED at a time around the ring)
  - `pio run -e wave -t upload` (first BCM traveling wave; button cycles modes)
- Plain `pio run` builds only the default env (`ornament`); name the others
  with `-e` when checking that everything still builds.
- To add a program: create `src/<name>.c` with its own `main()`, then add an
  `[env:<name>]` in `platformio.ini` with
  `build_src_filter = +<board.c> +<name>.c` (plus any shared modules it uses).
- Warnings: `build_src_flags = -Wall -Wextra` applies to project sources only
  (the ST HAL itself has unused-parameter warnings). Keep it warning-free.
- No floating point in firmware: after a build,
  `arm-none-eabi-nm .pio/build/ornament/firmware.elf | grep __aeabi_[fd]`
  must print nothing.
- Host tests (no hardware): `make -C host_test` runs `test_wave` and
  `test_ornament` (every mode for 60 s, every crossfade, button, flash log,
  gauge; per-slice LED limit, cap, determinism).
- Previews: `python3 tools/preview.py [mode numbers | gauge]` renders
  `docs/previews/*.gif` from the real code via `host_test/render`.
- Tables: `python3 tools/gen_tables.py` regenerates `src/tables.c`
  (gamma, inverse gamma, sine, exp). Don't edit `tables.c` by hand.
- Upload goes through the STM32CubeProgrammer CLI (`upload_protocol = custom`,
  path in `custom_cubeprog_cli` in `platformio.ini`). OpenOCD's flash
  algorithm times out on this board. The STM32CubeProgrammer app must be
  closed while uploading (only one program can hold the ST-LINK).
- Programmer: the ST-LINK/V2-1 on a NUCLEO-L476RG (CN2 jumpers removed),
  wired to the ornament's P1 header with jumper wires. The coin cell must be
  in and the slide switch on while flashing; the ST-LINK does not power the board.
- **Any debugger session (OpenOCD or the CubeProgrammer CLI) holds the target
  in reset while attached.** Reading RAM or registers over SWD therefore shows
  reset state, not the running firmware, so live profiling/inspection over SWD
  doesn't work on this setup. Don't try it; verify behaviour on the host and
  by eye. The firmware runs normally once the session ends or the Nucleo is
  unplugged.
- If the ornament is asleep (Stop mode) SWD can't connect. The firmware never
  enters Stop within 10 s of a reset, so: slide switch off/on, flash within 10 s.
- Alternatively flash `.pio/build/<env>/firmware.bin` at 0x08000000 with
  the STM32CubeProgrammer app.

## Hardware facts

- MCU: STM32F103RBT6, LQFP64, 128 KB flash, 20 KB RAM.
- Clock: internal 8 MHz HSI only. There is no crystal. Don't configure HSE.
- Supply: CR2477 (3.0 V nominal, sags under load) -> slide switch SW2 -> 3.3V net.
  No regulator. The "3.3V" net is really the battery voltage (about 2.7-3.2 V).
- LEDs: Kingbright APHCM2012CGCK-F01 (green, 0805, Vf about 1.9-2.1 V).
  Each LED: GPIO -> 220 ohm -> anode, cathode -> GND. GPIO high = on, about 5 mA.
- LED order: index 0..39 = D1..D40, clockwise viewed from the LED side,
  9 degrees apart, D40 at 12 o'clock. Pin map is in `include/led_pins.h`
  (used by `board.c`, `bcm_frame.c` and `ring_frame.c`) and was checked against the Altium PCB.
- Button SW1 (Omron B3U-1000P): PB15 to GND. Use the internal pull-up; pressed reads low.
- BOOT0: 10k pull-down to GND (boots from flash).
- NRST: on P1 pin 1, but not wired to the programmer.
- Unused pins: PC13-PC15, PB13, PB14, PD0, PD1, PD2.

### Programming header P1 (Samtec FTSH-105, 2x5 1.27 mm) - NON-STANDARD pinout

| P1 pin | Signal | | P1 pin | Signal |
|---|---|---|---|---|
| 1 | NRST | | 2 | SWCLK |
| 3 | NC | | 4 | SWDIO |
| 5 | GND | | 6 | GND |
| 7 | 3.3V | | 8 | 3.3V |
| 9 | NC | | 10 | NC |

This does not match the ARM Cortex 10-pin standard, so a standard SWD cable
will not work plugged straight in. Wiring through an Adafruit #2743 breakout:
Nucleo CN4 pin 1 (VDD_TARGET) -> P1-7, CN4-2 (SWCLK) -> P1-2,
CN4-3 (GND) -> P1-5, CN4-4 (SWDIO) -> P1-4.

## Rules that must not be broken

1. **Current limit.** The STM32F103's absolute max total current into VDD is
   150 mA. All 40 LEDs at 5 mA = 200 mA, which would exceed it and can't be
   fixed in hardware. Never have more than `LED_MAX_ON` (20) LEDs drawing full
   current at the same instant. `led_set()` enforces this; don't bypass it by
   writing GPIO registers directly unless the replacement enforces the same
   budget. With PWM/BCM brightness, the limit applies to every time slice:
   count the pins that are high during each slice, not average brightness.
   In `ornament`, the banked engine enforces it structurally (one 20-LED bank
   per slice; checked by static asserts and `test_ornament`). Keep it that way.
2. **Keep SWD alive.** PA15, PB3, PB4 drive LEDs, so JTAG must be turned off
   with `__HAL_AFIO_REMAP_SWJ_NOJTAG()` (SWJ_CFG = 010). Never use
   `__HAL_AFIO_REMAP_SWJ_DISABLE()` and never reconfigure PA13/PA14. Doing so
   locks out the programmer, since NRST isn't wired to it.
3. **PC13-PC15** can only sink 3 mA and must not source current. They are
   unused; keep it that way.
4. Never flash a quick test that turns every pin high.

## Battery reality

A CR2477 is rated about 1000 mAh at very light loads (Panasonic standard
drain 0.2 mA). At the tens of mA an animation draws, its voltage sags and
usable capacity drops a lot. Keep average current low: dim the wave, light as
few LEDs as the effect needs, and sleep whenever possible.

## `ornament` architecture

- **Engine** (`ring_frame.c` pure C, `ring_engine.c` board layer): banked
  7-bit BCM on TIM2. Bank A = even LED indices, bank B = odd (20 each).
  14 slices per frame (A6 B6 ... A0 B0), LSB 128 ticks, 246 Hz. Each slice
  drives one bank only and the ISR clears (BRR) before it sets (BSRR), so at
  most 20 LEDs are ever high, structurally, for any pattern. Double buffered;
  the swap happens only at slice 0. Gamma (`TBL_GAMMA`) and the global cap
  (`BRIGHTNESS_CAP_PCT` 30, code 76/127) are applied in the engine.
- **Modes** (`src/modes/`, one file each, listed in `mode_table.c`):
  `init(seed)` and `update(frame_ms, out[40])` writing perceptual 0..255.
  Fixed 16 ms frames. Shared helpers in `include/fx.h` / `src/modes/fx.c`
  and geometry in `include/ring_geom.h` (ring units: 256 per LED, 10240 per
  turn, clockwise from the top). Integer/fixed point only, xorshift32 PRNG.
- **Core** (`ornament_core.c`): mode switching, 300 ms crossfades, battery
  gauge overlay at start-up.
- **Button** (`button_fsm.c`): 25 ms debounce; short (reported 350 ms after
  release), double, long (1 s, fires while held); the wake press is ignored.
- **Power** (`power.c`): unused pins PC13-15, PB13, PB14, PD2 to analog
  (PD0/PD1 left alone, since remapping them needs AFIO_MAPR); VDD via
  VREFINT (`VREFINT_MV`, ±3-5 %); Stop mode with the low-power regulator,
  wake on PB15 falling edge via EXTI15 (`AFIO->EXTICR[3]`, never MAPR);
  clears DBG_STOP/DBG_STANDBY before Stop.
- **Mode memory** (`mode_store.c` + `power.c`): append-only halfword log in
  the last flash page (0x0801FC00, 512 records of `0xA5<<8 | mode`), erased
  only when full. `board_upload.maximum_size = 130048` keeps code out of it.
- **Main loop** (`ornament.c`): auto-sleep after 20 min without a press;
  save 60 s after a mode change and on sleep; 10 s Stop guard after reset.

## Roadmap

1. `blink_one` - single LED bring-up test.  (done)
2. `ring_chase` - one LED at a time; verifies all 40 LEDs and the button.  (done)
3. `wave` - 8-bit BCM traveling wave on TIM2 with per-plane limiting.  (done)
4. `ornament` v1.0 - banked BCM engine, 20 modes, double press to mode 1,
   battery gauge, long-press and 20-minute auto-sleep in Stop mode with
   button wake, mode memory in flash.  (done, tested on hardware)

## Style

- C, ST HAL. Keep hardware access in the board layer (`board.c`, `led_bcm.c`,
  `ring_engine.c`, `power.c` and their headers); animation code and modes
  should only call the board API or write their `out[]` buffer.
- Build with `-Wall -Wextra` and keep it warning-free.
