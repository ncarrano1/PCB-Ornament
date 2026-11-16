# PCB Ornament firmware

Firmware for a round PCB Christmas ornament: an STM32F103RBT6 driving a ring of
40 green LEDs, powered by one CR2477 coin cell. The goal is a smooth "traveling
wave" of brightness around the ring, a mode button, and an auto-sleep timer.

The hardware is built and cannot change. All constraints below come from the
real board, so treat them as fixed.

## Build and flash

- PlatformIO, `framework = stm32cube` (ST HAL). One environment per program:
  - `pio run -e blink_one -t upload` (blinks D1)
  - `pio run -e ring_chase -t upload` (one LED at a time around the ring)
  - `pio run -e wave -t upload` (BCM traveling wave; button cycles modes)
- To add a program: create `src/<name>.c` with its own `main()`, then add an
  `[env:<name>]` in `platformio.ini` with
  `build_src_filter = +<board.c> +<name>.c` (plus any shared modules it uses,
  e.g. `wave` also lists `led_bcm.c`, `bcm_frame.c`, `wave_anim.c`).
- Warnings: `build_src_flags = -Wall -Wextra` applies to project sources only
  (the ST HAL itself has unused-parameter warnings).
- Host test (no hardware): `make -C host_test`. It checks the wave generator
  and BCM bit-plane builder never exceed `LED_MAX_ON` in any bit-plane.
- Programmer: the ST-LINK/V2-1 on a NUCLEO-L476RG (CN2 jumpers removed),
  wired to the ornament's P1 header with jumper wires. The coin cell must be
  in and the slide switch on while flashing; the ST-LINK does not power the board.
- Alternatively flash `.pio/build/<env>/firmware.bin` at 0x08000000 with
  STM32CubeProgrammer.

## Hardware facts

- MCU: STM32F103RBT6, LQFP64, 128 KB flash, 20 KB RAM.
- Clock: internal 8 MHz HSI only. There is no crystal. Don't configure HSE.
- Supply: CR2477 (3.0 V nominal, sags under load) -> slide switch SW2 -> 3.3V net.
  No regulator. The "3.3V" net is really the battery voltage (about 2.7-3.2 V).
- LEDs: Kingbright APHCM2012CGCK-F01 (green, 0805, Vf about 1.9-2.1 V).
  Each LED: GPIO -> 220 ohm -> anode, cathode -> GND. GPIO high = on, about 5 mA.
- LED order: index 0..39 = D1..D40, clockwise viewed from the LED side,
  9 degrees apart, D40 at 12 o'clock. Pin map is in `include/led_pins.h`
  (used by `board.c` and `bcm_frame.c`) and was checked against the Altium PCB.
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

## Roadmap

1. `blink_one` - single LED bring-up test.  (done)
2. `ring_chase` - one LED at a time; verifies all 40 LEDs and the button.  (done)
3. Traveling wave: 8-bit BCM on TIM2 at 245 Hz (8 MHz HSI, no PLL),
   per-plane current limit and 30 % cap in `bcm_frame.c`.  (`wave`; built, awaiting hardware test)
4. Mode button: slow -> fast -> two waves -> off.  (`wave`; built, awaiting hardware test)
5. Sleep timer: after about 20 minutes, turn LEDs off and enter Stop mode
   (microamp draw). Wake on the button (EXTI on PB15).

## Style

- C, ST HAL. Keep hardware access in the board layer (`board.c`, `led_bcm.c`
  and their headers); animation programs
  should only call the board API.
- Build with `-Wall -Wextra` and keep it warning-free.
