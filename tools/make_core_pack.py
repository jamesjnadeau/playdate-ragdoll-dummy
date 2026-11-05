#!/usr/bin/env python3
"""Generates the placeholder 'core' pack: 1-bit RGBA PNGs, WAVs and pack.json.

Art is drawn at 4x on a (colour, alpha) pair and downsampled with a threshold, so every
output pixel is pure black, pure white, or fully transparent. Fully deterministic: all
randomness uses fixed seeds, so re-running produces byte-identical files.

Usage: python3 tools/make_core_pack.py [--out Source]
"""
import argparse
import json
import math
import os
import random
import struct
import wave

from PIL import Image, ImageDraw

SS = 4  # supersampling factor
BLACK, WHITE = 0, 255
MEDIUM_H = 120.0


class Canvas:
    """Draw in logical (output) pixel coordinates; stored at SS x resolution."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.col = Image.new("L", (w * SS, h * SS), WHITE)
        self.alpha = Image.new("L", (w * SS, h * SS), 0)
        self.dc = ImageDraw.Draw(self.col)
        self.da = ImageDraw.Draw(self.alpha)

    def _p(self, pts):
        return [(x * SS, y * SS) for x, y in pts]

    def _box(self, b):
        return [b[0] * SS, b[1] * SS, b[2] * SS, b[3] * SS]

    def _both(self, fn, color):
        fn(self.dc, color)
        fn(self.da, 255)

    def ellipse(self, box, color):
        b = self._box(box)
        self._both(lambda d, c: d.ellipse(b, fill=c), color)

    def rrect(self, box, r, color):
        b = self._box(box)
        self._both(lambda d, c: d.rounded_rectangle(b, radius=r * SS, fill=c), color)

    def rect(self, box, color):
        b = self._box(box)
        self._both(lambda d, c: d.rectangle(b, fill=c), color)

    def poly(self, pts, color):
        p = self._p(pts)
        self._both(lambda d, c: d.polygon(p, fill=c), color)

    def line(self, pts, width, color):
        """Polyline with round joints and caps."""
        p = self._p(pts)
        w = max(1, int(round(width * SS)))
        self._both(lambda d, c: d.line(p, fill=c, width=w, joint="curve"), color)
        r = w / 2.0
        for x, y in p:
            b = [x - r, y - r, x + r, y + r]
            self._both(lambda d, c: d.ellipse(b, fill=c), color)

    def arc(self, box, a0, a1, width, color):
        b = self._box(box)
        w = max(1, int(round(width * SS)))
        self._both(lambda d, c: d.arc(b, a0, a1, fill=c, width=w), color)

    def erase(self, box_fn):
        """Make a region fully transparent via a callback on the alpha draw."""
        box_fn(self.da)

    # Outlined shapes: black shape, then white shape inset by the outline width.
    def o_ellipse(self, box, ow, fill=WHITE):
        self.ellipse(box, BLACK)
        self.ellipse((box[0] + ow, box[1] + ow, box[2] - ow, box[3] - ow), fill)

    def o_rrect(self, box, r, ow, fill=WHITE):
        self.rrect(box, r, BLACK)
        self.rrect((box[0] + ow, box[1] + ow, box[2] - ow, box[3] - ow), max(0.5, r - ow), fill)

    def to_image(self):
        """Threshold-downsample to a 1-bit RGBA image."""
        a = self.alpha.resize((self.w, self.h), Image.BOX)
        # premultiplied colour so transparent areas do not bias edges
        prem = Image.new("L", self.col.size, 0)
        prem.paste(self.col, mask=self.alpha)
        c = prem.resize((self.w, self.h), Image.BOX)
        out = Image.new("RGBA", (self.w, self.h), (0, 0, 0, 0))
        ap, cp, op = a.load(), c.load(), out.load()
        for y in range(self.h):
            for x in range(self.w):
                av = ap[x, y]
                if av >= 128:
                    v = 255 if cp[x, y] * 255 >= 128 * av else 0
                    op[x, y] = (v, v, v, 255)
        return out


def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=False)


# ---------------------------------------------------------------- dummy parts

def ow_for(k):
    return max(1.5, 2.0 * k)


def sc(v, k):
    return max(1, int(round(v * k)))


def draw_body(k):
    w, h = sc(46, k), sc(72, k)
    ow = ow_for(k)
    c = Canvas(w, h)
    # slightly barrel-shaped punching bag
    c.o_rrect((0, 0, w, h), 17 * k, ow)
    # dashed vertical seam
    x = w / 2.0
    y = 8 * k
    while y < h - 6 * k:
        c.line([(x, y), (x, y + 3 * k)], 1.0 * max(k, 0.8), BLACK)
        y += 6 * k
    # X-stitched square patch
    ps = 16 * k
    px0, py0 = w / 2.0 - ps / 2.0 + 0 * k, h * 0.50
    c.rect((px0, py0, px0 + ps, py0 + ps), BLACK)
    c.rect((px0 + 1.2 * k, py0 + 1.2 * k, px0 + ps - 1.2 * k, py0 + ps - 1.2 * k), WHITE)
    m = 3.2 * k
    c.line([(px0 + m, py0 + m), (px0 + ps - m, py0 + ps - m)], 1.2 * max(k, 0.8), BLACK)
    c.line([(px0 + ps - m, py0 + m), (px0 + m, py0 + ps - m)], 1.2 * max(k, 0.8), BLACK)
    return c.to_image()


def draw_head(k):
    s = sc(38, k)
    ow = ow_for(k)
    c = Canvas(s, s)
    c.o_ellipse((0, 0, s, s), ow)
    # a little stitched crown seam
    c.arc((s * 0.22, s * 0.10, s * 0.78, s * 0.70), 215, 325, 1.0 * max(k, 0.8), BLACK)
    return c.to_image()


def draw_arm(k, w0, h0, lower):
    w, h = sc(w0, k), sc(h0, k)
    ow = ow_for(k) * 0.9
    c = Canvas(w, h)
    c.o_rrect((0, 0, w, h), w / 2.0 - 0.2, ow)
    if not lower:
        # sleeve band near the top
        c.line([(ow, h * 0.30), (w - ow, h * 0.30)], 1.0 * max(k, 0.8), BLACK)
    return c.to_image()


def draw_fist(k):
    s = sc(20, k)
    ow = ow_for(k) * 0.9
    c = Canvas(s, s)
    # mitten: round palm with a thumb bump on the left, outlined as one union
    palm = (s * 0.14, s * 0.08, s * 0.94, s * 0.92)
    thumb = (0.0, s * 0.34, s * 0.44, s * 0.70)
    c.ellipse(palm, BLACK)
    c.ellipse(thumb, BLACK)
    c.ellipse((palm[0] + ow, palm[1] + ow, palm[2] - ow, palm[3] - ow), WHITE)
    c.ellipse((thumb[0] + ow, thumb[1] + ow, thumb[2] - ow, thumb[3] - ow), WHITE)
    c.line([(s * 0.62, s * 0.22), (s * 0.62, s * 0.50)], 1.0 * max(k, 0.8), BLACK)
    return c.to_image()


def draw_base(k):
    w, h = sc(58, k), sc(22, k)
    c = Canvas(w, h)
    c.ellipse((0, 0, w, h), BLACK)
    # white highlight arc on the upper-left rim
    c.arc((w * 0.10, h * 0.14, w * 0.90, h * 0.86), 185, 262, 1.6 * max(k, 0.8), WHITE)
    return c.to_image()


# ---- faces: 34x30 at medium, scaled with k

def face(k, fn):
    w, h = sc(34, k), sc(30, k)
    c = Canvas(w, h)
    # logical drawing space is the 34x30 medium grid; wrap to scale by k
    fn(Scaled(c, k))
    return c.to_image()


class Scaled:
    """Forwards drawing calls with all logical coordinates multiplied by k."""

    def __init__(self, c, k):
        self.c, self.k = c, k

    def _pts(self, pts):
        return [(x * self.k, y * self.k) for x, y in pts]

    def _b(self, b):
        return tuple(v * self.k for v in b)

    def ellipse(self, b, col):
        self.c.ellipse(self._b(b), col)

    def rrect(self, b, r, col):
        self.c.rrect(self._b(b), r * self.k, col)

    def poly(self, p, col):
        self.c.poly(self._pts(p), col)

    def line(self, p, w, col):
        self.c.line(self._pts(p), w * max(self.k, 0.8), col)

    def arc(self, b, a0, a1, w, col):
        self.c.arc(self._b(b), a0, a1, w * max(self.k, 0.8), col)


def face_mean(d):
    d.line([(4, 6), (14, 11.5)], 3.0, BLACK)
    d.line([(30, 6), (20, 11.5)], 3.0, BLACK)
    for ex in (10, 24):
        d.ellipse((ex - 3, 14.5, ex + 3, 20.5), BLACK)
        d.ellipse((ex - 1.9, 15.4, ex - 0.2, 17.1), WHITE)
    # wide grin with zigzag teeth
    pts = []
    for i in range(0, 21):
        t = math.pi * i / 20.0
        pts.append((17 - 11 * math.cos(t), 22 + 6.5 * math.sin(t)))
    d.poly(pts, BLACK)
    inner = []
    for i in range(0, 21):
        t = math.pi * i / 20.0
        inner.append((17 - 9.6 * math.cos(t), 22.9 + 4.6 * math.sin(t)))
    d.poly(inner, WHITE)
    zz = []
    n = 8
    for i in range(n + 1):
        zz.append((7.6 + 18.8 * i / n, 23.6 if i % 2 == 0 else 26.4))
    d.line(zz, 1.2, BLACK)
    d.line([(7.4, 22.9), (26.6, 22.9)], 1.2, BLACK)


def face_hurt(d):
    d.line([(6, 10.5), (13, 14.5), (6, 18.5)], 2.6, BLACK)
    d.line([(28, 10.5), (21, 14.5), (28, 18.5)], 2.6, BLACK)
    d.ellipse((13.2, 19.5, 20.8, 28.5), BLACK)
    d.ellipse((14.8, 21.0, 17.0, 23.0), WHITE)


def spiral(cx, cy, r, turns):
    pts = []
    steps = 40
    for i in range(steps + 1):
        t = i / float(steps)
        a = t * turns * 2 * math.pi
        rr = r * t
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return pts


def face_dazed(d):
    d.line(spiral(9.5, 13.5, 6.2, 2.0), 1.5, BLACK)
    d.line(spiral(24.5, 13.5, 6.2, -2.0), 1.5, BLACK)
    pts = []
    for i in range(0, 25):
        x = 8 + 18 * i / 24.0
        pts.append((x, 24.5 + 2.0 * math.sin(i / 24.0 * 3 * 2 * math.pi)))
    d.line(pts, 2.0, BLACK)


def face_ko(d):
    for ex in (9.5, 24.5):
        d.line([(ex - 4, 9.5), (ex + 4, 17.5)], 2.6, BLACK)
        d.line([(ex + 4, 9.5), (ex - 4, 17.5)], 2.6, BLACK)
    d.line([(9, 23), (25, 23)], 2.2, BLACK)
    # tongue lolling out of the right side
    d.rrect((17.5, 22.5, 24.5, 29.5), 3.0, BLACK)
    d.rrect((18.8, 22.5, 23.2, 28.2), 2.0, WHITE)
    d.line([(21, 23.5), (21, 26.5)], 0.9, BLACK)
    d.line([(9, 23), (25, 23)], 2.2, BLACK)


def face_whack(d):
    # eyes rolled up: white with the pupils at the top
    for ex in (10, 24):
        d.ellipse((ex - 4.6, 7.5, ex + 4.6, 18.0), BLACK)
        d.ellipse((ex - 3.4, 8.7, ex + 3.4, 16.8), WHITE)
        d.ellipse((ex - 1.9, 8.7, ex + 1.9, 12.4), BLACK)
    # wide open mouth with a gap-toothed top row
    d.ellipse((8, 19, 26, 30), BLACK)
    d.rrect((10.4, 19.4, 14.4, 23.2), 0.8, WHITE)
    d.rrect((19.6, 19.4, 23.6, 23.2), 0.8, WHITE)


def face_squish(d):
    d.line([(4.5, 14), (14.5, 14)], 2.6, BLACK)
    d.line([(19.5, 14), (29.5, 14)], 2.6, BLACK)
    d.ellipse((12, 22.2, 22, 26.2), BLACK)


FACES = [("face-mean", face_mean), ("face-hurt", face_hurt), ("face-dazed", face_dazed),
         ("face-ko", face_ko), ("face-whack", face_whack), ("face-squish", face_squish)]

SIZES = [("dummy-small", 84.0), ("dummy-medium", 120.0), ("dummy-large", 156.0)]


def make_dummies(root):
    for folder, h in SIZES:
        k = h / MEDIUM_H
        d = os.path.join(root, folder)
        save(draw_body(k), os.path.join(d, "body.png"))
        save(draw_head(k), os.path.join(d, "head.png"))
        save(draw_arm(k, 11, 22, False), os.path.join(d, "arm-upper.png"))
        save(draw_arm(k, 11, 20, True), os.path.join(d, "arm-lower.png"))
        save(draw_fist(k), os.path.join(d, "fist.png"))
        save(draw_base(k), os.path.join(d, "base.png"))
        for name, fn in FACES:
            save(face(k, fn), os.path.join(d, name + ".png"))


# --------------------------------------------------------------------- weapons

WW, WH = 60, 190


def draw_chicken():
    c = Canvas(WW, WH)
    # legs (the grip) with webbed feet at the bottom
    c.line([(24, 150), (22, 176)], 5, BLACK)
    c.line([(36, 150), (38, 176)], 5, BLACK)
    for fx in (22, 38):
        c.poly([(fx, 172), (fx - 10, 187), (fx + 10, 187)], BLACK)
        c.poly([(fx, 176), (fx - 6.5, 185), (fx + 6.5, 185)], WHITE)
    # body
    c.o_ellipse((6, 78, 54, 158), 2.6)
    # wing with feather scallops
    c.o_ellipse((8, 100, 30, 142), 2.2)
    for y in (108, 118, 128):
        c.arc((11, y, 27, y + 12), 20, 160, 1.4, BLACK)
    # tail feathers
    c.poly([(10, 100), (0, 86), (8, 84), (3, 70), (16, 86)], BLACK)
    c.poly([(11, 98), (3, 87), (9, 86), (6, 76), (15, 88)], WHITE)
    # neck and head
    c.o_rrect((22, 40, 40, 90), 7, 2.4)
    c.o_ellipse((16, 14, 46, 46), 2.6)
    # comb, beak, wattle, eye
    for cx, cy in ((25, 10), (31, 6), (37, 10)):
        c.ellipse((cx - 5, cy - 4, cx + 5, cy + 5), BLACK)
    c.poly([(45, 24), (59, 31), (45, 37)], BLACK)
    c.poly([(46, 27), (55, 31), (46, 34)], WHITE)
    c.ellipse((42, 36, 48, 46), BLACK)
    c.ellipse((33, 24, 39, 30), BLACK)
    c.ellipse((34.5, 24.6, 36.5, 26.6), WHITE)
    return c


def draw_bat():
    c = Canvas(WW, WH)
    cx = 30
    # body: narrow handle at the bottom flaring to a fat barrel at the top
    outline = [(cx - 5, 184), (cx - 5, 118), (cx - 7, 96), (cx - 12, 60), (cx - 14, 22),
               (cx - 11, 7), (cx - 5, 2), (cx + 5, 2), (cx + 11, 7), (cx + 14, 22),
               (cx + 12, 60), (cx + 7, 96), (cx + 5, 118), (cx + 5, 184)]
    c.poly(outline, BLACK)
    inner = [(cx - 3, 182), (cx - 3, 117), (cx - 5, 95), (cx - 10, 60), (cx - 12, 22),
             (cx - 9, 9), (cx - 4, 4.5), (cx + 4, 4.5), (cx + 9, 9), (cx + 12, 22),
             (cx + 10, 60), (cx + 5, 95), (cx + 3, 117), (cx + 3, 182)]
    c.poly(inner, WHITE)
    c.rrect((cx - 5, 178, cx + 5, 188), 3, BLACK)  # knob
    # wood grain
    for gx, y0, y1 in ((-6, 14, 40), (4, 22, 52), (-3, 56, 80), (7, 44, 70)):
        c.line([(cx + gx, y0), (cx + gx + 1, y1)], 1.0, BLACK)
    # taped grip
    c.poly([(cx - 4, 176), (cx - 4, 134), (cx + 4, 134), (cx + 4, 176)], BLACK)
    for y in range(138, 176, 8):
        c.line([(cx - 3.5, y + 4), (cx + 3.5, y)], 1.6, WHITE)
    return c


def draw_pan():
    c = Canvas(WW, WH)
    cx = 30
    # handle
    c.o_rrect((cx - 5.5, 70, cx + 5.5, 186), 5, 2.2, fill=BLACK)
    c.ellipse((cx - 2.2, 170, cx + 2.2, 179), WHITE)
    c.line([(cx - 1.4, 84), (cx - 1.4, 150)], 1.2, WHITE)
    # pan body: dark dish with a rim and a shine
    c.ellipse((2, 2, 58, 74), BLACK)
    c.ellipse((6, 6, 54, 70), WHITE)
    c.ellipse((10, 10, 50, 66), BLACK)
    c.arc((15, 15, 45, 61), 200, 285, 2.4, WHITE)
    c.ellipse((40, 22, 44, 26), WHITE)
    return c


def draw_sledge():
    c = Canvas(WW, WH)
    cx = 30
    # handle
    c.o_rrect((cx - 6, 44, cx + 6, 188), 4, 2.2)
    for y in range(120, 186, 9):
        c.line([(cx - 5, y + 4), (cx + 5, y)], 1.2, BLACK)
    c.rrect((cx - 6, 178, cx + 6, 190), 3, BLACK)
    # head: heavy block with steel highlights
    c.o_rrect((2, 4, 58, 46), 5, 3)
    c.rect((2 + 11, 4.2, 2 + 13.5, 45.8), BLACK)
    c.rect((58 - 13.5, 4.2, 58 - 11, 45.8), BLACK)
    c.rrect((2, 4, 13, 46), 4, BLACK)
    c.rrect((47, 4, 58, 46), 4, BLACK)
    c.line([(20, 12), (40, 12)], 2.2, BLACK)
    c.line([(20, 22), (36, 22)], 1.4, BLACK)
    return c


WEAPONS = [("weapon-chicken", draw_chicken), ("weapon-bat", draw_bat),
           ("weapon-pan", draw_pan), ("weapon-sledge", draw_sledge)]


def measure_weapon(img):
    """Returns (grip, tip) in image pixels, origin top-left, measured from opaque pixels."""
    px = img.load()
    w, h = img.size
    rows = []
    for y in range(h):
        xs = [x for x in range(w) if px[x, y][3] > 0]
        rows.append(xs)
    ys = [y for y in range(h) if rows[y]]
    top, bottom = ys[0], ys[-1]
    tip_xs = rows[top]
    tip = [int(round(sum(tip_xs) / float(len(tip_xs)))), top]
    gy = bottom - 16
    gxs = rows[gy] if rows[gy] else rows[bottom]
    grip = [int(round(sum(gxs) / float(len(gxs)))), gy]
    return grip, tip


def make_icon(c):
    """32x32 icon: the held art tilted 45 degrees, fitted into 28x28, thresholded."""
    col = c.col.rotate(45, resample=Image.BICUBIC, expand=True, fillcolor=WHITE)
    alpha = c.alpha.rotate(45, resample=Image.BICUBIC, expand=True, fillcolor=0)
    alpha = alpha.point(lambda v: 255 if v >= 128 else 0)
    bb = alpha.getbbox()
    col, alpha = col.crop(bb), alpha.crop(bb)
    k = 28.0 / max(alpha.size)
    nw, nh = max(1, int(round(alpha.width * k))), max(1, int(round(alpha.height * k)))
    a = alpha.resize((nw, nh), Image.BOX)
    prem = Image.new("L", col.size, 0)
    prem.paste(col, mask=alpha)
    prem = prem.resize((nw, nh), Image.BOX)
    out = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    po, pa, pp = out.load(), a.load(), prem.load()
    ox, oy = (32 - nw) // 2, (32 - nh) // 2
    for y in range(nh):
        for x in range(nw):
            av = pa[x, y]
            if av >= 128:
                v = 255 if pp[x, y] * 255 >= 128 * av else 0
                po[ox + x, oy + y] = (v, v, v, 255)
    return out


def make_weapons(root):
    anchors = {}
    for folder, fn in WEAPONS:
        cv = fn()
        img = cv.to_image()
        d = os.path.join(root, folder)
        save(img, os.path.join(d, "held.png"))
        save(make_icon(cv), os.path.join(d, "icon.png"))
        grip, tip = measure_weapon(img)
        anchors[folder] = {"grip": grip, "tip": tip}
    return anchors


# -------------------------------------------------------------------- stuffing

def make_stuffing(root):
    d = os.path.join(root, "stuffing")
    for i, size in enumerate((8, 10, 12, 14)):
        rng = random.Random(1000 + i)
        c = Canvas(size, size)
        circles = []
        n = 3 + (i % 3)
        for j in range(n):
            a = 2 * math.pi * j / n + rng.uniform(-0.4, 0.4)
            dist = size * rng.uniform(0.14, 0.24)
            r = size * rng.uniform(0.22, 0.28)
            circles.append((size / 2.0 + dist * math.cos(a), size / 2.0 + dist * math.sin(a), r))
        circles.append((size / 2.0, size / 2.0, size * 0.26))
        for cx, cy, r in circles:
            c.ellipse((cx - r, cy - r, cx + r, cy + r), BLACK)
        ow = 1.0
        for cx, cy, r in circles:
            r2 = max(0.5, r - ow)
            c.ellipse((cx - r2, cy - r2, cx + r2, cy + r2), WHITE)
        save(c.to_image(), os.path.join(d, "puff-%d.png" % (i + 1)))
    for i, size in enumerate((10, 13, 15, 18)):
        rng = random.Random(2000 + i)
        c = Canvas(size, size)
        n = 7 + i
        cx = cy = size / 2.0
        pts = []
        for j in range(n):
            a = 2 * math.pi * j / n + rng.uniform(-0.15, 0.15)
            r = size * (0.46 if j % 2 == 0 else 0.30) * rng.uniform(0.85, 1.0)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a) * 0.8))
        c.poly(pts, BLACK)
        shrink = 1.0 - 1.1 / (size / 2.0)
        c.poly([(cx + (x - cx) * shrink, cy + (y - cy) * shrink) for x, y in pts], WHITE)
        # dashed seam across the scrap
        a0 = rng.uniform(0, math.pi)
        dx, dy = math.cos(a0), math.sin(a0)
        reach = size * 0.28
        steps = max(2, int(reach * 2 / 2.4))
        for s in range(steps):
            t0 = -reach + (2 * reach) * s / float(steps)
            if s % 2 == 0:
                t1 = t0 + 1.2
                c.line([(cx + dx * t0, cy + dy * t0), (cx + dx * t1, cy + dy * t1)], 0.8, BLACK)
        save(c.to_image(), os.path.join(d, "scrap-%d.png" % (i + 1)))


# -------------------------------------------------------------------------- UI

def make_ui(root):
    d = os.path.join(root, "ui")

    def heart_poly(w, h, inset):
        pts = []
        # parametric heart scaled to the box
        for i in range(60):
            t = 2 * math.pi * i / 60.0
            x = 16 * math.sin(t) ** 3
            y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
            pts.append((x, -y))
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        sx = (w - 2 * inset) / (max(xs) - min(xs))
        sy = (h - 2 * inset) / (max(ys) - min(ys))
        return [(inset + (x - min(xs)) * sx, inset + (y - min(ys)) * sy) for x, y in pts]

    c = Canvas(14, 12)
    c.poly(heart_poly(14, 12, 0.0), BLACK)
    c.ellipse((2.6, 2.4, 4.8, 4.2), WHITE)  # glint
    save(c.to_image(), os.path.join(d, "heart-full.png"))
    c = Canvas(14, 12)
    c.poly(heart_poly(14, 12, 0.0), BLACK)
    c.poly(heart_poly(14, 12, 1.4), WHITE)
    save(c.to_image(), os.path.join(d, "heart-empty.png"))

    c = Canvas(16, 16)
    c.line([(8, 0.5), (8, 3)], 1.2, BLACK)
    c.o_rrect((3, 3, 13, 15.5), 3.2, 1.3)
    c.line([(3.5, 6.2), (12.5, 6.2)], 1.1, BLACK)
    c.line([(5.5, 10.5), (10.5, 12.5)], 0.9, BLACK)
    c.line([(10.5, 10.5), (5.5, 12.5)], 0.9, BLACK)
    save(c.to_image(), os.path.join(d, "bag.png"))


# ---------------------------------------------------------------------- sounds

RATE = 22050


def write_wav(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    peak = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s / peak * 0.85)) * 32767))
                    for s in samples)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


def lowpass(xs, a):
    out, y = [], 0.0
    for x in xs:
        y += a * (x - y)
        out.append(y)
    return out


def thump(rng, dur, f0, f1, noise_amt, decay):
    n = int(RATE * dur)
    out, ph = [], 0.0
    for i in range(n):
        t = i / float(n)
        f = f0 + (f1 - f0) * min(1.0, t * 3)
        ph += 2 * math.pi * f / RATE
        env = math.exp(-t * decay)
        click = noise_amt * rng.uniform(-1, 1) * math.exp(-t * 40)
        out.append(env * math.sin(ph) + click)
    return out


def make_sounds(root):
    d = os.path.join(root, "sounds")
    rng = random.Random(77)
    # swing: noise with a sweeping low-pass and a bell envelope
    n = int(RATE * 0.28)
    out, y = [], 0.0
    for i in range(n):
        t = i / float(n)
        a = 0.04 + 0.5 * math.sin(math.pi * t) ** 2
        y += a * (rng.uniform(-1, 1) - y)
        out.append(y * math.sin(math.pi * t) ** 1.5)
    write_wav(os.path.join(d, "swing.wav"), out)
    write_wav(os.path.join(d, "hit-light.wav"), thump(rng, 0.14, 170, 70, 0.6, 7))
    write_wav(os.path.join(d, "hit-heavy.wav"), thump(rng, 0.32, 110, 38, 0.9, 5))
    # burst: decaying low-passed noise
    n = int(RATE * 0.25)
    burst = lowpass([rng.uniform(-1, 1) * math.exp(-4.5 * i / n) for i in range(n)], 0.45)
    write_wav(os.path.join(d, "burst.wav"), burst)
    # grunts: downward-gliding buzzy blips
    for gi, (f0, f1) in enumerate(((190, 120), (150, 95), (230, 140))):
        n = int(RATE * 0.16)
        ph, out = 0.0, []
        for i in range(n):
            t = i / float(n)
            ph += 2 * math.pi * (f0 + (f1 - f0) * t) / RATE
            s = math.sin(ph) + 0.5 * math.sin(2 * ph) + 0.25 * math.sin(3 * ph)
            out.append(s * math.sin(math.pi * min(1.0, t * 1.2)) * (1 - t * 0.6))
        write_wav(os.path.join(d, "grunt-%d.wav" % (gi + 1)), lowpass(out, 0.5))
    # door-slam: deep thump plus a noisy crack and a rumble tail
    n = int(RATE * 0.5)
    base = thump(rng, 0.5, 90, 35, 1.2, 6)
    tail = lowpass([rng.uniform(-1, 1) * math.exp(-7 * i / n) for i in range(n)], 0.2)
    write_wav(os.path.join(d, "door-slam.wav"), [a + 0.6 * b for a, b in zip(base, tail)])
    write_wav(os.path.join(d, "step.wav"), thump(rng, 0.09, 140, 60, 0.4, 12))
    # hurt: an "oof" - a short falling thump with a buzzy, voiced body, lower and longer than the hit sounds
    n = int(RATE * 0.24)
    ph, out = 0.0, []
    for i in range(n):
        t = i / float(n)
        ph += 2 * math.pi * (130 - 60 * t) / RATE
        voice = math.sin(ph) + 0.6 * math.sin(2 * ph) + 0.3 * math.sin(3 * ph)
        env = min(1.0, t * 30) * math.exp(-t * 4.5)
        out.append(env * voice + 0.5 * rng.uniform(-1, 1) * math.exp(-t * 55))
    write_wav(os.path.join(d, "hurt.wav"), lowpass(out, 0.45))


# ------------------------------------------------------------------- pack.json

def make_manifest(root, weapon_anchors):
    anchors = {}
    for folder, h in SIZES:
        k = h / MEDIUM_H
        anchors[folder] = {
            "height_px": int(h),
            "neck": [0, round(-78 * k, 2)],
            "shoulder_l": [round(-20 * k, 2), round(-64 * k, 2)],
            "shoulder_r": [round(20 * k, 2), round(-64 * k, 2)],
            "face_center": [0, round(-94 * k, 2)],
            "body_center": [0, round(-45 * k, 2)],
            "upper_arm_px": round(20 * k, 2),
            "lower_arm_px": round(18 * k, 2),
        }
    anchors.update(weapon_anchors)
    manifest = {
        "spec": 1,
        "id": "core",
        "name": "Core",
        "version": "1.0.0",
        "author": "Ragdoll Dummies contributors",
        "license": "CC-BY-4.0",
        "provides": [f for f, _ in SIZES] + [f for f, _ in WEAPONS] + ["stuffing", "sounds"],
        "anchors": anchors,
    }
    path = os.path.join(root, "pack.json")
    with open(path, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "Source"))
    args = ap.parse_args()
    src = os.path.abspath(args.out)
    core = os.path.join(src, "packs", "core")
    make_dummies(core)
    wa = make_weapons(core)
    make_stuffing(core)
    make_sounds(core)
    make_manifest(core, wa)
    make_ui(src)
    for name, a in wa.items():
        print(name, a)


if __name__ == "__main__":
    main()
