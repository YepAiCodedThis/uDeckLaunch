#!/usr/bin/env python3
"""Build 5 fake library covers (1:1 and 2:3 capsule) with the uDeckLaunch logo."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
LOGO_CANDIDATES = [
    ROOT.parent / "uDeckLaunch-Logo.png",
    ROOT / "uDeckLaunch-Logo.png",
    ROOT / "assets" / "uDeckLaunch-Logo.png",
]
OUT = ROOT / "assets" / "privacy"

PALETTES = [
    # name, top-left, bottom-right, glow
    ((8, 18, 42), (26, 159, 255), (90, 200, 255)),       # deck blue
    ((42, 10, 14), (255, 106, 26), (255, 180, 90)),     # ember
    ((22, 8, 42), (168, 85, 247), (220, 160, 255)),     # violet
    ((6, 32, 22), (16, 185, 129), (120, 240, 180)),     # teal
    ((36, 18, 6), (234, 179, 8), (255, 220, 120)),      # gold
]


def load_logo() -> Image.Image:
    for p in LOGO_CANDIDATES:
        if p.is_file():
            im = Image.open(p).convert("RGBA")
            bbox = im.getbbox()
            if bbox:
                im = im.crop(bbox)
            return im
    raise FileNotFoundError("uDeckLaunch-Logo.png not found")


def lerp(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    t = 0.0 if t < 0 else 1.0 if t > 1 else t
    return (
        int(a[0] + (b[0] - a[0]) * t),
        int(a[1] + (b[1] - a[1]) * t),
        int(a[2] + (b[2] - a[2]) * t),
    )


def make_bg(w: int, h: int, c0, c1, glow, seed: int) -> Image.Image:
    try:
        import numpy as np
    except ImportError:
        np = None
    if np is None:
        im = Image.new("RGB", (w, h))
        px = im.load()
        ang = 0.55 + seed * 0.31
        ca, sa = math.cos(ang), math.sin(ang)
        span = abs(w * ca) + abs(h * sa) or 1.0
        gx = w * (0.35 + 0.08 * (seed % 3))
        gy = h * (0.38 + 0.07 * ((seed + 1) % 3))
        gr = min(w, h) * 0.85
        for y in range(h):
            for x in range(w):
                t = (x * ca + y * sa) / span + 0.5
                t = t * t * (3 - 2 * t)
                r, g, b = lerp(c0, c1, t)
                dx = (x - gx) / gr
                dy = (y - gy) / gr
                fall = math.exp(-0.55 * (dx * dx + dy * dy))
                r = int(r + (glow[0] - r) * fall * 0.35)
                g = int(g + (glow[1] - g) * fall * 0.35)
                b = int(b + (glow[2] - b) * fall * 0.35)
                vig = 1.0 - 0.28 * (((x / w - 0.5) ** 2) + ((y / h - 0.5) ** 2) * 1.4)
                px[x, y] = (max(0, int(r * vig)), max(0, int(g * vig)), max(0, int(b * vig)))
        return im

    yy, xx = np.mgrid[0:h, 0:w]
    ang = 0.55 + seed * 0.31
    ca, sa = math.cos(ang), math.sin(ang)
    span = abs(w * ca) + abs(h * sa) or 1.0
    t = (xx * ca + yy * sa) / span + 0.5
    t = np.clip(t, 0, 1)
    t = t * t * (3 - 2 * t)
    c0a = np.array(c0, dtype=np.float32)
    c1a = np.array(c1, dtype=np.float32)
    gl = np.array(glow, dtype=np.float32)
    rgb = c0a + (c1a - c0a) * t[..., None]
    gx = w * (0.35 + 0.08 * (seed % 3))
    gy = h * (0.38 + 0.07 * ((seed + 1) % 3))
    gr = min(w, h) * 0.85
    dx = (xx - gx) / gr
    dy = (yy - gy) / gr
    fall = np.exp(-0.55 * (dx * dx + dy * dy))[..., None]
    rgb = rgb + (gl - rgb) * fall * 0.35
    vig = 1.0 - 0.28 * (((xx / w - 0.5) ** 2) + ((yy / h - 0.5) ** 2) * 1.4)
    rgb *= vig[..., None]
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")


def paste_logo(bg: Image.Image, logo: Image.Image, cover: bool) -> Image.Image:
    w, h = bg.size
    max_w = int(w * (0.78 if cover else 0.82))
    max_h = int(h * (0.58 if cover else 0.78))
    scale = min(max_w / logo.width, max_h / logo.height)
    lw = max(1, int(logo.width * scale))
    lh = max(1, int(logo.height * scale))
    mark = logo.resize((lw, lh), Image.Resampling.LANCZOS)
    shadow = Image.new("RGBA", (lw, lh), (0, 0, 0, 0))
    alpha = mark.split()[3]
    shadow.putalpha(alpha.point(lambda a: int(a * 0.35)))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=max(4, lw // 40)))
    canvas = bg.convert("RGBA")
    x = (w - lw) // 2
    y = (h - lh) // 2
    canvas.paste(shadow, (x + lw // 40, y + lh // 28), shadow)
    canvas.paste(mark, (x, y), mark)
    return canvas.convert("RGB")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    logo = load_logo()
    for i, (c0, c1, glow) in enumerate(PALETTES, start=1):
        sq = paste_logo(make_bg(600, 600, c0, c1, glow, i), logo, cover=False)
        cap = paste_logo(make_bg(600, 900, c0, c1, glow, i + 7), logo, cover=True)
        sq.save(OUT / f"sq{i}.jpg", quality=92, optimize=True, progressive=True)
        cap.save(OUT / f"cap{i}.jpg", quality=92, optimize=True, progressive=True)
        print("wrote", f"sq{i}.jpg", f"cap{i}.jpg")


if __name__ == "__main__":
    main()
