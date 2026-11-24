#!/usr/bin/env python3
"""Render each ornament mode to an animated GIF in docs/previews/.

Runs the real mode code and brightness engine on the host (host_test/render),
so the GIFs show the actual duty codes the LEDs will be driven with,
including gamma, the brightness cap and 7-bit quantization.

Usage (from the repo root):
    python3 tools/preview.py            # all modes
    python3 tools/preview.py 2 3        # only modes 2 and 3
"""
import math
import os
import re
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RENDER = os.path.join(ROOT, "host_test", "render")
OUT_DIR = os.path.join(ROOT, "docs", "previews")

SECONDS = {"default": 10}  # loop length per mode name (seconds)
SIZE = 200                 # GIF width/height, px
RING_R = 80                # ring radius, px
DOT_R = 5                  # LED radius, px
LEVELS = 48                # intensity steps in the GIF palette (smaller files)
SAMPLE = 2                 # keep every 2nd frame (16 ms * 2 = 32 ms)
LEDS = 40


def build_renderer():
    subprocess.run(["make", "-s", "-C", os.path.join(ROOT, "host_test"), "render"], check=True)


def mode_list():
    out = subprocess.run([RENDER, "--list"], check=True, capture_output=True, text=True).stdout
    lines = out.strip().split("\n")
    cap, frame_ms = (int(x) for x in lines[0].split())
    return cap, frame_ms, lines[1:]


def led_xy(i):
    """LED i (D(i+1)) seen from the LED side: D40 at 12 o'clock, clockwise."""
    angle = ((i + 1) % LEDS) * 2 * math.pi / LEDS  # from the top, clockwise
    c = SIZE / 2
    return c + RING_R * math.sin(angle), c - RING_R * math.cos(angle)


def to_srgb(linear):
    """Linear light 0..1 -> sRGB-encoded 0..1, so the screen matches the LED."""
    if linear <= 0.0031308:
        return 12.92 * linear
    return 1.055 * linear ** (1 / 2.4) - 0.055


def green_palette():
    pal = []
    for k in range(256):
        v = k / 255
        # LED green (~570 nm), slightly desaturated toward white at the top.
        pal += [int(40 * v + 30 * v * v), int(255 * v), int(70 * v + 40 * v * v)]
    return pal


def frame_image(codes, cap):
    """One frame: soft glow plus crisp LED dots, as an 'L' intensity image."""
    glow = Image.new("L", (SIZE, SIZE), 0)
    dots = Image.new("L", (SIZE, SIZE), 0)
    gd, dd = ImageDraw.Draw(glow), ImageDraw.Draw(dots)
    for i in range(LEDS):
        x, y = led_xy(i)
        lin = codes[i] / cap            # 1.0 = full brightness under the cap
        lvl = int(255 * to_srgb(lin))
        unlit = 22                      # faint pad so the ring is visible
        gd.ellipse([x - DOT_R * 2.2, y - DOT_R * 2.2, x + DOT_R * 2.2, y + DOT_R * 2.2],
                   fill=int(lvl * 0.55))
        dd.ellipse([x - DOT_R, y - DOT_R, x + DOT_R, y + DOT_R], fill=max(unlit, lvl))
    glow = glow.filter(ImageFilter.GaussianBlur(5))
    img = ImageChops.lighter(dots, glow)
    step = 255 / (LEVELS - 1)
    return img.point(lambda v: int(round(v / step) * step))


def render_mode(num, name, cap, frame_ms):
    seconds = SECONDS.get(name, SECONDS["default"])
    raw = subprocess.run([RENDER, str(num), str(seconds * 1000)], check=True,
                         capture_output=True).stdout
    frames = [raw[k:k + LEDS] for k in range(0, len(raw), LEDS)]
    images, durations = [], []
    t = 0.0
    pal = green_palette()
    for n in range(0, len(frames), SAMPLE):
        img = frame_image(frames[n], cap).convert("P")
        img.putpalette(pal)
        images.append(img)
        # GIF delays are in 10 ms units: alternate 30/40 ms to stay real time.
        t_next = t + frame_ms * SAMPLE
        durations.append(int(t_next // 10 * 10 - t // 10 * 10))
        t = t_next
    slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    path = os.path.join(OUT_DIR, f"{num:02d}_{slug}.gif")
    images[0].save(path, save_all=True, append_images=images[1:], duration=durations,
                   loop=0, optimize=True)
    return path, len(images), seconds


def main():
    build_renderer()
    cap, frame_ms, names = mode_list()
    want = {int(a) for a in sys.argv[1:]} or set(range(1, len(names) + 1))
    os.makedirs(OUT_DIR, exist_ok=True)
    for num, name in enumerate(names, 1):
        if num in want:
            path, n, secs = render_mode(num, name, cap, frame_ms)
            kb = os.path.getsize(path) // 1024
            print(f"{os.path.relpath(path, ROOT)}: {n} frames, {secs} s, {kb} KB")


if __name__ == "__main__":
    main()
