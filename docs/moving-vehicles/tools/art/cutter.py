"""Cuts a painted view of a vehicle into X-COM tiles.

The vehicle's tile parts are turned to the view's facing (walls moving to the neighbouring tile where
turning puts them on a tile's far edge), each with its hit shape turned. Rendering the turned hit shapes
tells which part is in front at every pixel; each painted pixel goes to that part's sprite. Parts the view
doesn't show (inside the hull) keep their own sprite, turned where that can be done exactly. The result
is a tileset (MCD, PCK, TAB) and a map of the whole craft turned the same way.
"""
import os
import struct
import sys
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xcomart as xa
import template as tp
import evaluate as ev

FLOOR, WEST, NORTH, OBJECT = 0, 1, 2, 3


# --- turning geometry: one quarter turn sends north to east, tile (x, y) of a W x L box to (L - 1 - y, x) ---

def turn_tile(x, y, W, L, k):
    for _ in range(k):
        x, y, W, L = L - 1 - y, x, L, W
    return x, y


def turn_wall(slot, x, y, W, L, k):
    """A wall is an edge between tile corners; turned, it lands on some tile's west or north edge."""
    a, b = ((x, y), (x, y + 1)) if slot == WEST else ((x, y), (x + 1, y))
    for _ in range(k):
        a, b, W, L = (L - a[1], a[0]), (L - b[1], b[0]), L, W
    if a[0] == b[0]:
        return WEST, a[0], min(a[1], b[1])
    return NORTH, min(a[0], b[0]), a[1]


def turn_pattern(rows, k):
    """Turns a 16 x 16 hit shape layer (bit 15 - x of row y) like the tiles: (x, y) -> (15 - y, x)."""
    for _ in range(k):
        new = [0] * 16
        for y in range(16):
            for x in range(16):
                if rows[y] & (1 << (15 - x)):
                    nx, ny = 15 - y, x
                    new[ny] |= 1 << (15 - nx)
        rows = new
    return rows


def settle_wall(rows, slot):
    """A wall's shape belongs on the tile's near edge: mirror it there if turning left it on the far edge."""
    near = far = 0
    for y in range(16):
        for x in range(16):
            if rows[y] & (1 << (15 - x)):
                c = x if slot == WEST else y
                near += c < 8
                far += c >= 8
    if far <= near:
        return rows
    if slot == WEST:
        return [int('{:016b}'.format(r)[::-1], 2) for r in rows]
    return rows[::-1]


class Lofts:
    """Finds the hit shape layer in LOFTEMPS that matches a pattern, or the closest one."""

    def __init__(self):
        data = tp.loftemps()
        self.table = [tuple(data[i * 16:(i + 1) * 16]) for i in range(len(data) // 16)]
        self.cache = {}
        self.inexact = 0

    def find(self, rows):
        rows = tuple(rows)
        if rows in self.cache:
            return self.cache[rows]
        if not any(rows):
            self.cache[rows] = 0
            return 0
        best, best_d = 0, None
        for i, t in enumerate(self.table):
            d = sum(bin(a ^ b).count('1') for a, b in zip(rows, t))
            if best_d is None or d < best_d:
                best, best_d = i, d
                if d == 0:
                    break
        if best_d:
            self.inexact += 1
        self.cache[rows] = best
        return best


# --- the vehicles of a craft map ---

def pieces(tiles, terrain, sx, sy, sz):
    """Groups the map's tiles into vehicles as the engine's connected split does."""
    filled = {k for k, v in tiles.items() if any(terrain.part(p) for p in v)}
    seen, out = set(), []
    for start in sorted(filled):
        if start in seen:
            continue
        group, todo = set(), [start]
        seen.add(start)
        while todo:
            x, y, z = todo.pop()
            group.add((x, y, z))
            for dz in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        n = (x + dx, y + dy, z + dz)
                        if n in filled and n not in seen:
                            seen.add(n)
                            todo.append(n)
        out.append(group)
    return out


def cut(draft_path, facing_from, facing_to, terrain, tiles, group, lofts, colours, sz):
    """Cuts a painted view onto one vehicle. Returns its parts: dicts with the original part, where it goes
    (turned, in the vehicle's box), its new hit shape layers and its 32 x 40 sprite."""
    k = ((facing_to - facing_from) % 8) // 2
    x0 = min(p[0] for p in group)
    y0 = min(p[1] for p in group)
    W = max(p[0] for p in group) - x0 + 1
    L = max(p[1] for p in group) - y0 + 1
    nW, nL = (L, W) if k % 2 else (W, L)

    parts = []
    for (x, y, z) in sorted(group):
        for slot in range(4):
            got = terrain.part(tiles[(x, y, z)][slot])
            if not got:
                continue
            name, index, rec, pck = got
            lx, ly = x - x0, y - y0
            if slot in (WEST, NORTH):
                nslot, nx, ny = turn_wall(slot, lx, ly, W, L, k)
            else:
                nslot, (nx, ny) = slot, turn_tile(lx, ly, W, L, k)
            layers = []
            for layer in range(12):
                rows = list(tp.loftemps()[rec['loft'][layer] * 16:(rec['loft'][layer] + 1) * 16]) if rec['loft'][layer] else [0] * 16
                rows = turn_pattern(rows, k)
                if nslot in (WEST, NORTH):
                    rows = settle_wall(rows, nslot)
                layers.append(rows)
            parts.append({'src': (name, index), 'rec': rec, 'pck': pck, 'from': (x, y, z, slot),
                          'x': nx, 'y': ny, 'z': z, 'slot': nslot, 'layers': layers, 'k': k})

    # which part is in front, voxel by voxel (objects win over walls over floors)
    wx, wy, wz = (nW + 1) * 16, (nL + 1) * 16, sz * 24
    owner = [-1] * (wx * wy * wz)
    for pid in sorted(range(len(parts)), key=lambda i: parts[i]['slot'] == OBJECT):
        p = parts[pid]
        for layer, rows in enumerate(p['layers']):
            for vy in range(16):
                if not rows[vy]:
                    continue
                for vx in range(16):
                    if rows[vy] & (1 << (15 - vx)):
                        X, Y = p['x'] * 16 + vx, p['y'] * 16 + vy
                        for vz in (layer * 2, layer * 2 + 1):
                            owner[((p['z'] * 24 + vz) * wy + Y) * wx + X] = pid

    # render: for each pixel the part in front (screen u = X - Y, v = (X + Y) // 2 - Z)
    u0, v0 = -(wy - 1), -wz
    width, height = wx + wy, (wx + wy) // 2 + wz + 2
    own = [-1] * (width * height)
    for v in range(v0, v0 + height):
        for u in range(u0, u0 + width):
            for Z in range(wz - 1, -1, -1):
                X = (u + 2 * (v + Z)) // 2
                Y = X - u
                if X >= wx or Y >= wy:
                    continue
                if X < 0 or Y < 0:
                    break
                o = owner[(Z * wy + Y) * wx + X]
                if o >= 0:
                    own[(v - v0) * width + (u - u0)] = o
                    break
    cols = [i % width for i in range(width * height) if own[i] >= 0]
    rows_ = [i // width for i in range(width * height) if own[i] >= 0]
    cx0, cx1, cy0, cy1 = min(cols), max(cols) + 1, min(rows_), max(rows_) + 1
    tw, th = cx1 - cx0, cy1 - cy0
    crop_own = [own[(cy0 + j) * width + cx0 + i] for j in range(th) for i in range(tw)]

    # the painting, at game scale on the template's box
    idx, _ = ev.fit(draft_path, tw, th, colours)

    # each painted pixel goes to the part in front there, or the nearest one
    nearest = list(crop_own)
    queue = deque(i for i in range(tw * th) if crop_own[i] >= 0)
    while queue:
        i = queue.popleft()
        x, y = i % tw, i // tw
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx_, ny_ = x + dx, y + dy
            if 0 <= nx_ < tw and 0 <= ny_ < th and nearest[ny_ * tw + nx_] < 0:
                nearest[ny_ * tw + nx_] = nearest[i]
                queue.append(ny_ * tw + nx_)

    def frame_origin(p):
        return 16 * (p['x'] - p['y']) - 16, 8 * (p['x'] + p['y']) - 24 * p['z'] - 24

    for p in parts:
        p['sprite'] = [0] * (32 * 40)
        p['painted'] = 0
    stray = 0
    for j in range(th):
        for i in range(tw):
            c = idx[j * tw + i]
            if not c:
                continue
            u, v = cx0 + i + u0, cy0 + j + v0
            pid = nearest[j * tw + i]
            fu, fv = frame_origin(parts[pid])
            fx, fy = u - fu, v - fv
            if not (0 <= fx < 32 and 0 <= fy < 40):
                # outside its part's sprite: the closest part whose sprite reaches here
                best = None
                for q, p in enumerate(parts):
                    qu, qv = frame_origin(p)
                    if 0 <= u - qu < 32 and 0 <= v - qv < 40:
                        d = abs(p['x'] - parts[pid]['x']) + abs(p['y'] - parts[pid]['y']) + abs(p['z'] - parts[pid]['z'])
                        if best is None or d < best[0]:
                            best = (d, q)
                if best is None:
                    stray += 1
                    continue
                pid = best[1]
                fu, fv = frame_origin(parts[pid])
                fx, fy = u - fu, v - fv
            parts[pid]['sprite'][fy * 32 + fx] = c
            parts[pid]['painted'] += 1

    # parts the view doesn't show keep their own sprite: a floor turned round exactly, walls mirrored
    # when they change sides, anything else as it was
    hidden = 0
    for p in parts:
        if p['painted']:
            continue
        hidden += 1
        src = p['pck'][p['rec']['frames'][0]] if p['rec']['frames'][0] < len(p['pck']) else [0] * 1280
        yoff = p['rec']['yoffset']
        moved = [0] * 1280
        for j in range(40):
            for i in range(32):
                if j + yoff < 40:
                    moved[j * 32 + i] = src[(j + yoff) * 32 + i] if 0 <= j + yoff else 0
        if p['slot'] == FLOOR and p['k'] == 2:
            out = [0] * 1280
            for j in range(40):
                for i in range(32):
                    c = moved[j * 32 + i]
                    if c and j >= 24:
                        out[(63 - j) * 32 + 31 - i] = c
                    elif c:
                        out[j * 32 + i] = c
            moved = out
        elif p['slot'] != p['from'][3] or p['k'] % 2:
            moved = [moved[j * 32 + 31 - i] for j in range(40) for i in range(32)]
        p['sprite'] = moved
    return parts, (nW, nL), {'stray': stray, 'hidden': hidden, 'painted': sum(1 for p in parts if p['painted'])}


# --- writing the game files ---

def encode_pck(frames):
    """X-COM PCK: rows of transparency skipped first, 0xFE n skips n pixels, 0xFF ends the frame."""
    pck, tab = bytearray(), []
    for px in frames:
        tab.append(len(pck))
        first = 0
        while first < 40 and not any(px[first * 32:(first + 1) * 32]):
            first += 1
        body = bytearray([first])
        run = 0
        last = max([i for i, c in enumerate(px) if c] or [0])
        for i in range(first * 32, last + 1):
            c = px[i]
            if c == 0:
                run += 1
                if run == 255:
                    body += bytes([0xFE, 255])
                    run = 0
                continue
            if run:
                body += bytes([0xFE, run])
                run = 0
            body.append(c if c < 0xFE else 0xFD)
        body.append(0xFF)
        pck += body
    return bytes(pck), b''.join(struct.pack('<H', o) for o in tab)


def mcd_record(part, frame, lofts):
    r = bytearray(part['rec']['raw'])
    for i in range(8):
        r[i] = frame
    for layer in range(12):
        r[8 + layer] = lofts.find(part['layers'][layer])
    r[30] = 0   # no UFO door
    r[35] = 0   # no door
    r[44] = 0   # no destroyed version
    r[46] = 0   # no alternative part
    r[49] = 0   # sprites are cut to the tile, no offset
    r[53] = part['slot']
    return bytes(r)


def write_map(path, sx, sy, sz, values):
    out = bytearray([sy, sx, sz])
    for z in range(sz - 1, -1, -1):
        for y in range(sy):
            for x in range(sx):
                out += bytes(values.get((x, y, z), [0, 0, 0, 0]))
    open(path, 'wb').write(out)
