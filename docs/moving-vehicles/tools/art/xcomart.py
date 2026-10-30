"""Reads X-COM terrain art (MAP, MCD, PCK/TAB, palette) and renders maps like the battlescape."""
import os
import re
import struct
import zlib

MODS = r'C:\Users\USUARIO\Documents\OpenXcom\mods'
UFO = r'C:\Users\USUARIO\source\repos\OpenXcom\bin\UFO'
SEARCH = [os.path.join(MODS, 'rosigma'), os.path.join(MODS, '40k'), UFO]
W, H = 32, 40  # frame size


def find(folder, name, exts):
    for root in SEARCH:
        d = os.path.join(root, folder)
        if not os.path.isdir(d):
            continue
        files = {f.lower(): f for f in os.listdir(d)}
        for ext in exts:
            f = files.get((name + ext).lower())
            if f:
                return os.path.join(d, f)
    raise FileNotFoundError(folder + '/' + name)


def palette():
    data = open(os.path.join(UFO, 'GEODATA', 'PALETTES.DAT'), 'rb').read()
    base = 4 * (256 * 3 + 6)  # the battlescape palette
    pal = [tuple(min(255, data[base + i * 3 + c] * 4) for c in range(3)) for i in range(256)]
    # Rosigma's own colours on top
    text = open(os.path.join(MODS, 'rosigma', 'Ruleset', 'palette.rul'), encoding='utf-8').read()
    for m in re.finditer(r'^\s+(\d+):\s*\[(\d+),\s*(\d+),\s*(\d+)\]', text, re.M):
        pal[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
    return pal


def load_pck(name):
    pck = open(find('TERRAIN', name, ['.pck']), 'rb').read()
    tab = open(find('TERRAIN', name, ['.tab']), 'rb').read()
    offsets = [struct.unpack_from('<H', tab, i)[0] for i in range(0, len(tab) - 1, 2)]
    frames = []
    for i, start in enumerate(offsets):
        end = offsets[i + 1] if i + 1 < len(offsets) else len(pck)
        px = [0] * (W * H)
        pos = pck[start] * W
        j = start + 1
        while j < end:
            b = pck[j]
            if b == 0xFF:
                break
            if b == 0xFE:
                pos += pck[j + 1]
                j += 2
                continue
            if pos < W * H:
                px[pos] = b
            pos += 1
            j += 1
        frames.append(px)
    return frames


def load_mcd(name):
    data = open(find('TERRAIN', name, ['.mcd']), 'rb').read()
    recs = []
    for i in range(0, len(data) - 61, 62):
        r = data[i:i + 62]
        recs.append({
            'frames': list(r[0:8]),
            'loft': list(r[8:20]),
            'bigwall': r[33],
            'yoffset': r[49],
            'type': r[53],
            'raw': r,
        })
    return recs


def load_map(name):
    data = open(find('MAPS', name, ['.MAP', '.map']), 'rb').read()
    sy, sx, sz = data[0], data[1], data[2]
    tiles = {}
    i = 3
    for z in range(sz - 1, -1, -1):
        for y in range(sy):
            for x in range(sx):
                tiles[(x, y, z)] = list(data[i:i + 4])
                i += 4
    return sx, sy, sz, tiles


class Terrain:
    """The tilesets of a terrain, in order, as a map's part numbers index them."""

    def __init__(self, datasets):
        self.sets = []
        for name in datasets:
            self.sets.append((name, load_mcd(name), load_pck(name)))

    def part(self, value):
        """MCD record and frames of a map value, or None."""
        if value == 0:
            return None
        for name, mcd, pck in self.sets:
            if value < len(mcd):
                return name, value, mcd[value], pck
            value -= len(mcd)
        return None


def render(terrain, sx, sy, sz, tiles, pal, transform=None):
    """Draws a map as the battlescape does: back to front, level by level. Returns (width, height, rgba)."""
    ox = sy * 16
    width = (sx + sy) * 16 + 32
    height = (sx + sy) * 8 + sz * 24 + 48
    buf = bytearray(width * height * 4)
    for z in range(sz):
        for y in range(sy):
            for x in range(sx):
                parts = tiles.get((x, y, z))
                if not parts:
                    continue
                for p in range(4):
                    got = terrain.part(parts[p])
                    if not got:
                        continue
                    _, _, rec, pck = got
                    frame = rec['frames'][0]
                    if frame >= len(pck):
                        continue
                    px = pck[frame]
                    sxp = ox + (x - y) * 16
                    syp = (x + y) * 8 + (sz - 1 - z) * 24 - rec['yoffset']
                    for j in range(H):
                        for i in range(W):
                            c = px[j * W + i]
                            if c == 0:
                                continue
                            X, Y = sxp + i, syp + j
                            if 0 <= X < width and 0 <= Y < height:
                                k = (Y * width + X) * 4
                                buf[k:k + 4] = bytes(pal[c]) + b'\xff'
    return width, height, buf


def write_png(path, width, height, rgba, scale=1):
    if scale > 1:
        big = bytearray(width * scale * height * scale * 4)
        for y in range(height * scale):
            for x in range(width * scale):
                s = ((y // scale) * width + x // scale) * 4
                d = (y * width * scale + x) * 4
                big[d:d + 4] = rgba[s:s + 4]
        width, height, rgba = width * scale, height * scale, big
    raw = b''.join(b'\x00' + bytes(rgba[y * width * 4:(y + 1) * width * 4]) for y in range(height))

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def crop(width, height, rgba):
    """Cuts away the empty border."""
    xs, ys = [], []
    for y in range(height):
        for x in range(width):
            if rgba[(y * width + x) * 4 + 3]:
                xs.append(x)
                ys.append(y)
    if not xs:
        return width, height, rgba
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    out = bytearray()
    for y in range(y0, y1):
        out += rgba[(y * width + x0) * 4:(y * width + x1) * 4]
    return x1 - x0, y1 - y0, out
