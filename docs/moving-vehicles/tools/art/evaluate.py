"""Fits a painted draft onto its template: shrinks it to game scale, brings it into the Chimera's colours,
and measures how well its outline matches the template's (and the real art's match, as a baseline)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xcomart as xa
import template as tp
from draft import read_png

OUT = os.path.join(HERE, 'out')
DRAFTS = os.path.join(HERE, 'drafts')
FACING = {'south': 4, 'east': 2, 'north': 0, 'west': 6}


def template_mask(facing):
    terrain = xa.Terrain(['BLANKS', 'CHIMERA', 'HIDRA', 'LEMON', 'REPULSOR'])
    sx, sy, sz, tiles = xa.load_map('CHIMERA_SMALL_NON_HYDRA')
    model = tp.voxels(terrain, tiles, (0, 1, 4, 8, sz))
    w, h, rgba = xa.crop(*tp.render(*tp.turn(*model, 4, FACING[facing])))
    return w, h, [rgba[i * 4 + 3] > 0 for i in range(w * h)]


def chimera_colours():
    pal = xa.palette()
    used = set()
    for name in ['CHIMERA', 'HIDRA']:
        for frame in xa.load_pck(name):
            used.update(frame)
    used.discard(0)
    return [(i, pal[i]) for i in sorted(used)]


def fit(path, tw, th, colours, magenta_cut=150):
    """Shrinks a draft's vehicle to tw x th game pixels. Returns (indices or 0, rgba)."""
    w, h, rgba = read_png(path)
    def is_vehicle(x, y):
        k = (y * w + x) * 4
        r, g, b = rgba[k], rgba[k + 1], rgba[k + 2]
        return ((r - 255) ** 2 + g ** 2 + (b - 255) ** 2) ** 0.5 > magenta_cut
    xs, ys = [], []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if is_vehicle(x, y):
                xs.append(x)
                ys.append(y)
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    out_idx = [0] * (tw * th)
    out = bytearray(tw * th * 4)
    for j in range(th):
        for i in range(tw):
            sx0 = x0 + (x1 - x0) * i // tw
            sx1 = max(sx0 + 1, x0 + (x1 - x0) * (i + 1) // tw)
            sy0 = y0 + (y1 - y0) * j // th
            sy1 = max(sy0 + 1, y0 + (y1 - y0) * (j + 1) // th)
            acc, n, total = [0, 0, 0], 0, 0
            # the middle of the cell, away from the blur of its edges
            mx0, mx1 = sx0 + (sx1 - sx0) // 4, sx1 - (sx1 - sx0) // 4
            my0, my1 = sy0 + (sy1 - sy0) // 4, sy1 - (sy1 - sy0) // 4
            for y in range(my0, max(my1, my0 + 1)):
                for x in range(mx0, max(mx1, mx0 + 1)):
                    total += 1
                    if is_vehicle(x, y):
                        k = (y * w + x) * 4
                        acc[0] += rgba[k]
                        acc[1] += rgba[k + 1]
                        acc[2] += rgba[k + 2]
                        n += 1
            if n * 2 < total:
                continue
            c = (acc[0] / n, acc[1] / n, acc[2] / n)
            idx, col = min(colours, key=lambda ic: sum((ic[1][q] - c[q]) ** 2 for q in range(3)))
            out_idx[j * tw + i] = idx
            out[(j * tw + i) * 4:(j * tw + i) * 4 + 4] = bytes(col) + b'\xff'
    return out_idx, out


def iou(a, b):
    inter = sum(1 for p, q in zip(a, b) if p and q)
    union = sum(1 for p, q in zip(a, b) if p or q)
    return inter / union if union else 0


def overlay(tw, th, mask, rgba):
    """The fitted draft with the template's outline drawn over it in white."""
    out = bytearray(rgba)
    for j in range(th):
        for i in range(tw):
            if not mask[j * tw + i]:
                continue
            edge = any(not (0 <= i + dx < tw and 0 <= j + dy < th and mask[(j + dy) * tw + i + dx])
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                out[(j * tw + i) * 4:(j * tw + i) * 4 + 4] = b'\xff\xff\xff\xff'
    return out


if __name__ == '__main__':
    colours = chimera_colours()
    # baseline: the real art against its own template
    tw, th, tmask = template_mask('south')
    w, h, real = read_png(os.path.join(OUT, 'chimera_south_one.png'))
    # the reference is at 4x: back to 1x by sampling
    w1, h1 = w // 4, h // 4
    rmask = [real[((j * 4 + 2) * w + i * 4 + 2) * 4 + 3] > 0 for j in range(h1) for i in range(w1)]
    print('real south art %dx%d vs template %dx%d' % (w1, h1, tw, th))
    for name in sys.argv[1:]:
        facing = name.split('_')[1]
        tw, th, tmask = template_mask(facing)
        idx, rgba = fit(os.path.join(DRAFTS, name + '.conv.png'), tw, th, colours)
        dmask = [i != 0 for i in idx]
        print(name, 'outline match (IoU) %.2f' % iou(tmask, dmask))
        xa.write_png(os.path.join(DRAFTS, name + '.game.png'), tw, th, rgba, 4)
        xa.write_png(os.path.join(DRAFTS, name + '.overlay.png'), tw, th, overlay(tw, th, tmask, rgba), 4)
