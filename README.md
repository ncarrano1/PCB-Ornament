# PCB Ornament

Firmware for a 40-LED PCB Christmas ornament built around an STM32F103RBT6,
powered by a CR2477 coin cell.

| Program | What it does |
|---|---|
| `blink_one` | Blinks D1 once a second. First bring-up test. |
| `ring_chase` | Lights one LED at a time around the ring (D1 to D40). Press the button to change speed. |
| `wave` | A smooth wave of brightness travels clockwise around the ring. Press the button to cycle: slow wave, fast wave, two opposite waves, off. |

## Setup (macOS)

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE**
   extension (or `brew install platformio` for the command line).
2. Clone this repo and open the folder in VS Code.

## Wiring the programmer

The programmer is the ST-LINK built into a NUCLEO-L476RG.

1. On the Nucleo, remove both **CN2** jumpers.
2. Plug the Adafruit #1675 cable onto the ornament's P1 header (red stripe to
   pin 1) and the other end into the Adafruit #2743 breakout.
3. Connect by pin number. P1's pinout is non-standard, so ignore the labels
   printed on the breakout.

   | Nucleo CN4 | Signal | Ornament P1 |
   |---|---|---|
   | 1 | VDD_TARGET (sense) | 7 |
   | 2 | SWCLK | 2 |
   | 3 | GND | 5 |
   | 4 | SWDIO | 4 |

4. Put the coin cell in and turn the ornament's slide switch on.
5. Plug the Nucleo into USB.

## Build and flash

```bash
pio run -e blink_one -t upload
pio run -e ring_chase -t upload
pio run -e wave -t upload
```

Or build with `pio run -e <env>` and flash `.pio/build/<env>/firmware.bin`
at address `0x08000000` with STM32CubeProgrammer (target STM32F103RB).

## Host test

The wave generator and BCM bit-plane builder are plain C, so they can be
tested on the Mac without the board:

```bash
make -C host_test
```

It checks every position of a full rotation in every mode and confirms that no
bit-plane ever has more than 20 LEDs on.

Wave shape and speed are set by constants at the top of `src/wave_anim.c`.
The global brightness cap (`BCM_CAP_Q8`, about 30 %) is in `include/bcm_frame.h`.

## Important

Never turn on more than 20 LEDs at full current at once: 40 x 5 mA exceeds
the STM32's 150 mA limit. See `CLAUDE.md` for this and the other hardware rules.
