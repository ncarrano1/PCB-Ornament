#!/usr/bin/env python3
"""Printable PCB Ornament card: 4 copies on one US-letter page (2 x 2)."""
import math
import sys

import matplotlib
matplotlib.use("pdf")
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
from matplotlib.patches import Circle, FancyBboxPatch, Polygon, Ellipse

matplotlib.rcParams["pdf.fonttype"] = 42

SUP = "/System/Library/Fonts/Supplemental/"
F_TITLE = FontProperties(fname=SUP + "Trebuchet MS Bold.ttf")
F_HEAD = FontProperties(fname=SUP + "Trebuchet MS Bold.ttf")
F_BODY = FontProperties(fname=SUP + "Trebuchet MS.ttf")
F_BOLD = FontProperties(fname=SUP + "Trebuchet MS Bold.ttf")
F_ITAL = FontProperties(fname=SUP + "Trebuchet MS Italic.ttf")

RED = "#c0262d"
DKRED = "#8e1a20"
GREEN = "#1f7a3a"
DKGREEN = "#145226"
GOLD = "#c9a227"
ICE = "#9cc3e6"
INK = "#2b2b2b"

URL = "https://github.com/ncarrano1/PCB-Ornament"

MODES = ["Wave", "Comet", "Binary stars", "Twinkle", "Pendulum", "Raindrops",
         "Heartbeat", "Candle", "Plasma", "Fireworks", "Standing wave", "Marble",
         "Snowfall", "Hourglass", "Lightning", "Fireflies", "Ripples", "Automaton",
         "Marquee", "Ember"]

BUTTON = [
    ("Press", "next mode"),
    ("Double press", "back to mode 1"),
    ("Hold 1 second", "sleep"),
    ("Press while asleep", "wake up"),
]
NOTES = [
    "At power-on the ring fills up like a battery gauge.",
    "Falls asleep by itself after 20 minutes.",
]
FACTS = [
    ("Average LED draw", "~5 mA, about 16 mW (0.3 to 17 mA, by mode)"),
    ("Asleep", "~20 \u00b5A"),
    ("Flash", "18.8 KB of 128 KB used (15 %)"),
    ("RAM", "2.6 KB of 20 KB used (13 %)"),
    ("Speed", "40 LEDs redrawn 246 times a second, animated at 62.5 fps"),
    ("Safety", "never more than 20 of the 40 LEDs fully on at once"),
]

W, H = 4.25, 5.5          # one card, inches
INSET = 0.27              # frame inset from the card edge (printer margin)


def snowflake(ax, x, y, r, color=ICE, lw=0.8):
    for k in range(6):
        a = math.pi / 2 + k * math.pi / 3
        ex, ey = x + r * math.cos(a), y + r * math.sin(a)
        ax.plot([x, ex], [y, ey], color=color, lw=lw, solid_capstyle="round")
        for f in (0.55,):
            bx, by = x + f * r * math.cos(a), y + f * r * math.sin(a)
            for s in (-1, 1):
                b = a + s * math.pi / 4
                ax.plot([bx, bx + 0.35 * r * math.cos(b)], [by, by + 0.35 * r * math.sin(b)],
                        color=color, lw=lw * 0.8, solid_capstyle="round")


def holly_leaf(ax, x, y, length, angle, color=GREEN):
    """Spiky holly leaf from (x, y) pointing at `angle` (radians)."""
    pts_top, pts_bot = [], []
    n = 4
    for i in range(n * 2 + 1):
        t = i / (n * 2)
        w = 0.32 * length * math.sin(math.pi * t)
        w *= 1.25 if i % 2 else 0.8      # spikes
        pts_top.append((t * length, w))
        pts_bot.append((t * length, -w))
    pts = pts_top + pts_bot[::-1]
    ca, sa = math.cos(angle), math.sin(angle)
    pts = [(x + px * ca - py * sa, y + px * sa + py * ca) for px, py in pts]
    ax.add_patch(Polygon(pts, closed=True, fc=color, ec=DKGREEN, lw=0.6, zorder=4))
    ax.plot([x, x + length * 0.95 * ca], [y, y + length * 0.95 * sa], color=DKGREEN, lw=0.5, zorder=5)


def holly(ax, x, y, s=1.0, flip=False):
    d = -1 if flip else 1
    holly_leaf(ax, x, y, 0.42 * s, math.radians(160 if flip else 20))
    holly_leaf(ax, x, y, 0.42 * s, math.radians(110 if flip else 70))
    for bx, by in ((0, 0), (0.07 * d, 0.05), (0.075 * d, -0.04)):
        ax.add_patch(Circle((x + bx * s, y + by * s), 0.045 * s, fc=RED, ec=DKRED, lw=0.5, zorder=6))
        ax.add_patch(Circle((x + bx * s - 0.013 * s, y + by * s + 0.014 * s), 0.011 * s,
                            fc="white", ec="none", alpha=0.8, zorder=7))


def ornament_icon(ax, cx, cy, r):
    """The ornament itself: 40 LEDs, D40 at the top, a travelling wave lit."""
    ax.add_patch(Circle((cx, cy), r * 1.22, fc="#b3202a", ec=DKRED, lw=1.2, zorder=3))
    # hanger cap and loop
    ax.add_patch(FancyBboxPatch((cx - 0.075, cy + r * 1.2), 0.15, 0.08,
                                boxstyle="round,pad=0.01", fc=GOLD, ec="#8a6d12", lw=0.6, zorder=4))
    ax.add_patch(Ellipse((cx, cy + r * 1.2 + 0.14), 0.09, 0.12, fc="none", ec="#8a6d12", lw=0.9, zorder=4))
    head = 13.0            # wave centre, in LEDs
    for i in range(40):
        a = math.pi / 2 - ((i + 1) % 40) * 2 * math.pi / 40   # clockwise from top
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        d = min(abs(i - head), 40 - abs(i - head))
        v = 0.5 + 0.5 * math.cos(math.pi * d / 7) if d < 7 else 0.0
        if v > 0.05:
            ax.add_patch(Circle((x, y), 0.035 + 0.04 * v, fc="#22d35a", ec="none", alpha=0.45 * v, zorder=4))
        g = (0.90 - 0.80 * v, 0.84 + 0.11 * v, 0.76 - 0.46 * v)
        ax.add_patch(Circle((x, y), 0.019 + 0.006 * v, fc=g, ec=DKRED, lw=0.25, zorder=5))


def candy_frame(ax, x0, y0, w, h):
    kw = dict(boxstyle="round,pad=0,rounding_size=0.18", fc="none", zorder=2)
    ax.add_patch(FancyBboxPatch((x0, y0), w, h, ec=RED, lw=6.5, **kw))
    ax.add_patch(FancyBboxPatch((x0, y0), w, h, ec="white", lw=6.5, ls=(0, (1.2, 1.6)), **kw))
    ax.add_patch(FancyBboxPatch((x0 + 0.075, y0 + 0.075), w - 0.15, h - 0.15,
                                boxstyle="round,pad=0,rounding_size=0.12", fc="none",
                                ec=GREEN, lw=0.8, zorder=2))


def card(ax, ox, oy):
    X = lambda x: ox + x
    Y = lambda y: oy + y
    L, R = INSET, W - INSET
    B, T = INSET, H - INSET
    candy_frame(ax, X(L), Y(B), R - L, T - B)

    # header
    ornament_icon(ax, X(0.80), Y(4.62), 0.27)
    ax.text(X(1.34), Y(4.74), "LED Ornament", fontproperties=F_TITLE, fontsize=25,
            color=RED, va="center", ha="left", zorder=6)
    ax.text(X(1.36), Y(4.45), "Merry Christmas & happy blinking!", fontproperties=F_ITAL,
            fontsize=8, color=GREEN, va="center", ha="left", zorder=6)
    ax.text(X(1.36), Y(4.31), "Love, Nico", fontproperties=F_ITAL,
            fontsize=8, color=GREEN, va="center", ha="left", zorder=6)
    for sx, sy, sr in ((3.72, 4.98, 0.07), (3.62, 4.30, 0.045), (1.25, 5.03, 0.04)):
        snowflake(ax, X(sx), Y(sy), sr)

    x_l = 0.48
    # button
    y = 4.03
    ax.text(X(x_l), Y(y), "HOW TO USE", fontproperties=F_HEAD, fontsize=8.5, color=RED, va="center")
    y -= 0.165
    for i, (k, v) in enumerate(BUTTON):
        xx = x_l + 0.05
        yy = y - i * 0.14
        ax.text(X(xx), Y(yy), "• ", fontproperties=F_BOLD, fontsize=7.2, color=GREEN, va="center")
        t = ax.text(X(xx + 0.09), Y(yy), f"{k}: ", fontproperties=F_BOLD, fontsize=7.2, color=INK, va="center")
        ax.annotate(v, xycoords=t, xy=(1, 0.5), va="center", fontproperties=F_BODY, fontsize=7.2, color=INK)
    y -= len(BUTTON) * 0.14
    for n in NOTES:
        ax.text(X(x_l + 0.05), Y(y), "• ", fontproperties=F_BOLD, fontsize=7.2, color=GREEN, va="center")
        ax.text(X(x_l + 0.14), Y(y), n, fontproperties=F_BODY, fontsize=7.2, color=INK, va="center")
        y -= 0.14

    # modes
    y -= 0.08
    ax.text(X(x_l), Y(y), "THE 20 MODES", fontproperties=F_HEAD, fontsize=8.5, color=RED, va="center")
    y -= 0.165
    cols = [x_l + 0.05, x_l + 1.17, x_l + 2.29]
    per = 7
    for i, name in enumerate(MODES):
        c, r = divmod(i, per)
        xx, yy = cols[c], y - r * 0.13
        ax.text(X(xx + 0.17), Y(yy), f"{i + 1}", fontproperties=F_BOLD, fontsize=7.2,
                color=GREEN, va="center", ha="right")
        ax.text(X(xx + 0.23), Y(yy), name, fontproperties=F_BODY, fontsize=7.2, color=INK, va="center")
    y -= per * 0.13 + 0.06

    # fun facts
    ax.text(X(x_l), Y(y), "FUN FACTS", fontproperties=F_HEAD, fontsize=8.5, color=RED, va="center")
    snowflake(ax, X(x_l + 0.85), Y(y), 0.045)
    y -= 0.165
    for k, v in FACTS:
        ax.text(X(x_l + 0.05), Y(y), "• ", fontproperties=F_BOLD, fontsize=7.0, color=GREEN, va="center")
        t = ax.text(X(x_l + 0.14), Y(y), f"{k}: ", fontproperties=F_BOLD, fontsize=7.0, color=INK, va="center")
        ax.annotate(v, xycoords=t, xy=(1, 0.5), va="center", fontproperties=F_BODY, fontsize=7.0, color=INK)
        y -= 0.128

    # footer
    holly(ax, X(R - 0.36), Y(B + 0.22), 0.8)
    holly(ax, X(L + 0.36), Y(B + 0.22), 0.8, flip=True)
    t = ax.text(X(W / 2), Y(B + 0.33), "Code, previews & how it works:", fontproperties=F_ITAL,
                fontsize=6.8, color=GREEN, ha="center", va="center")
    t = ax.text(X(W / 2), Y(B + 0.18), "github.com/ncarrano1/PCB-Ornament", fontproperties=F_BOLD,
                fontsize=7.6, color=RED, ha="center", va="center")
    t.set_url(URL)


def main(out):
    fig = plt.figure(figsize=(8.5, 11))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 8.5)
    ax.set_ylim(0, 11)
    ax.set_aspect("equal")
    ax.axis("off")
    for ox in (0, W):
        for oy in (0, H):
            card(ax, ox, oy)
    # cut lines
    ax.plot([W, W], [0, 11], color="#bbbbbb", lw=0.5, ls=(0, (4, 4)), zorder=1)
    ax.plot([0, 8.5], [H, H], color="#bbbbbb", lw=0.5, ls=(0, (4, 4)), zorder=1)
    fig.savefig(out, metadata={"Title": "LED Ornament card", "Author": None, "Creator": None,
                               "Producer": None})
    if len(sys.argv) > 2:
        fig.savefig(sys.argv[2], dpi=150)


if __name__ == "__main__":
    main(sys.argv[1])
