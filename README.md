# PCB Ornament

Firmware for a 40-LED PCB Christmas ornament built around an STM32F103RBT6,
powered by a CR2477 coin cell.

The main program is `ornament`: 20 animated modes on one button, a battery
gauge at power-up, sleep, and a mode that survives power-off.

| Program | What it does |
|---|---|
| `ornament` | **The final firmware** (default env). 20 modes, battery gauge, sleep, mode memory. |
| `blink_one` | Blinks D1 once a second. First bring-up test. |
| `ring_chase` | Lights one LED at a time around the ring (D1 to D40). Press the button to change speed. |
| `wave` | A smooth wave of brightness travels clockwise around the ring. Press the button to cycle: slow wave, fast wave, two opposite waves, off. |

## Using the ornament

| Button | Action |
|---|---|
| Single press | Next mode (takes effect about 0.35 s after release, to tell it apart from a double press) |
| Double press | Jump back to mode 1 |
| Hold 1 s | Sleep: the LEDs fade out and the chip enters Stop mode |
| Press while asleep | Wake up in the same mode |

- **Battery gauge:** at power-up and on every wake, the ring fills from the
  bottom like a liquid level to show the battery charge, then the mode fades
  in. It blinks if the battery is at 20 % or below.
- **Auto-sleep:** after 20 minutes without a button press.
- **Mode memory:** the current mode is saved to flash when the ornament goes
  to sleep, and also once a newly chosen mode has been left on for 60 s. The
  slide switch can turn it off any time and it comes back in the same mode.
- **Programming window:** after power-up the ornament never sleeps during the
  first 10 s, so the programmer can always connect after a slide-switch
  power cycle.

## Modes

The GIFs in [`docs/previews/`](docs/previews/) are rendered from the real
firmware code (including gamma, the 30 % brightness cap and 7-bit
quantization), so they show what the LEDs do. The mA column is the average
LED current from the host test over 60 s; the MCU itself adds a few mA while
awake.

| # | Mode | Description | LED mA |
|---|---|---|---|
| 1 | Wave | A smooth bump of light circles clockwise. | 4.3 |
| 2 | Comet | Bright head, fading tail, slowly speeding up and slowing down. | 3.2 |
| 3 | Binary stars | Two comets orbit in opposite directions and flare as they pass. | 3.1 |
| 4 | Twinkle | Fairy lights fading in and out at random. | 4.4 |
| 5 | Pendulum | A dot swings back and forth around the bottom. | 1.7 |
| 6 | Raindrops | Drops slide down both sides under gravity and splash at the bottom. | 3.8 |
| 7 | Heartbeat | A "lub-dub" pulse ripples from the top down both sides, 60 bpm. | 4.5 |
| 8 | Candle | A flame rising from the bottom: flickers, leans, and gusts. | 6.4 |
| 9 | Plasma | Bright bands that stream, merge and split. | 12.5 |
| 10 | Fireworks | A rocket climbs, bursts at the top, sparks fall and fade. | 1.4 |
| 11 | Standing wave | n wavelengths around the ring; nodes stay still, antinodes swell. | 17.3 |
| 12 | Marble | A ball rolls in a bowl, settles, then gets kicked again. | 1.4 |
| 13 | Snowfall | Flakes drift down and pile up at the bottom, then melt. | 5.4 |
| 14 | Hourglass | Sand trickles from the top into a pile, then the glass turns over. | 9.3 |
| 15 | Lightning | Mostly dark; stepped leaders, strikes, re-strikes and forks. | 0.3 |
| 16 | Fireflies | Wandering lights that gradually blink in unison. | 0.7 |
| 17 | Ripples | Drops land and send ripples both ways around the ring. | 3.2 |
| 18 | Automaton | 1-D cellular automata (rules 30, 90, 110, 150). | 11.7 |
| 19 | Marquee | Theatre-sign chase that sometimes pauses, reverses or races. | 12.0 |
| 20 | Ember | A few dim LEDs breathing at the bottom. Lowest-power mode. | 0.3 |

For the longest battery life, pick a low-current mode (lightning, fireflies,
ember, marble, fireworks) and let it sleep.

## Setup (macOS)

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE**
   extension (or `brew install platformio` for the command line).
2. Install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html).
   Uploads go through its command-line tool. If it is not installed in
   `/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/`, change
   `custom_cubeprog_cli` in `platformio.ini`.
3. Clone this repo and open the folder in VS Code.

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

If the ornament is asleep, the programmer can't connect: turn the slide switch
off and on, then flash within 10 s.

## Build and flash

```bash
pio run -t upload                  # ornament (the default)
pio run -e blink_one -t upload
pio run -e ring_chase -t upload
pio run -e wave -t upload
```

Or build with `pio run -e <env>` and flash `.pio/build/<env>/firmware.bin`
at address `0x08000000` with the STM32CubeProgrammer app (target STM32F103RB).
Close the app before uploading from PlatformIO; only one program can use the
ST-LINK at a time.

Unplug the Nucleo after flashing when testing sleep: while a debugger session
is attached the chip is held in reset.

## Host tests and previews

The animation code, brightness engine, button logic, flash log and gauge are
plain C, so they are tested on the Mac without the board:

```bash
make -C host_test                  # runs test_wave and test_ornament
python3 tools/preview.py           # re-render docs/previews/*.gif (needs Pillow)
python3 tools/preview.py 8 9       # only modes 8 and 9
python3 tools/preview.py gauge     # battery gauge previews
```

`test_ornament` runs every mode for 60 simulated seconds plus every mode-to-mode
crossfade, and checks that no time slice ever drives more than 20 LEDs, that
brightness stays under the cap, and that the output is deterministic.

## Adding or tweaking a mode

- Each mode is one file in `src/modes/`, with its tweakable constants at the top.
- To add one: create `src/modes/mode_<name>.c` exporting a `mode_def_t`
  (see `include/mode.h`), add one line to `src/modes/mode_table.c`, then run
  `make -C host_test` and `python3 tools/preview.py <number>`.
- Modes write perceptual brightness 0..255. The engine applies gamma and the
  global cap (`BRIGHTNESS_CAP_PCT` in `include/ring_frame.h`), so a mode can't
  break the current limit.

## Important

Never turn on more than 20 LEDs at full current at once: 40 x 5 mA exceeds
the STM32's 150 mA limit. See `CLAUDE.md` for this and the other hardware rules.
