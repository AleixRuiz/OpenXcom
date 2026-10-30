"""Makes the Chimera craft's north and west maps and tilesets from the painted views, and previews them."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xcomart as xa
import evaluate as ev
import cutter as ct

GEN = os.path.join(HERE, 'gen')
BASE = ['BLANKS', 'CHIMERA', 'HIDRA', 'LEMON', 'REPULSOR']
CRAFT_MAP = 'CHIMERA_SMALL_NON_HYDRA'
VIEWS = [  # facing, draft, tileset name, map name
    (0, os.path.join(HERE, 'drafts', '05_north_edit_0.conv.png'), 'MV_CHIMERA_N', CRAFT_MAP + '_N'),
    (6, os.path.join(HERE, 'drafts', '02_west_0.conv.png'), 'MV_CHIMERA_W', CRAFT_MAP + '_W'),
]
FRONT = 4  # the craft map's Chimeras face south

os.makedirs(os.path.join(GEN, 'TERRAIN'), exist_ok=True)
os.makedirs(os.path.join(GEN, 'MAPS'), exist_ok=True)
terrain = xa.Terrain(BASE)
sx, sy, sz, tiles = xa.load_map(CRAFT_MAP)
groups = ct.pieces(tiles, terrain, sx, sy, sz)
lofts = ct.Lofts()
colours = ev.chimera_colours()
print('vehicles on the craft map:', len(groups), 'tiles each:', [len(g) for g in groups])

for facing, draft, setname, mapname in VIEWS:
    k = ((facing - FRONT) % 8) // 2
    frames, records, values, known = [], [], {}, {}
    # a turned map lists its own tilesets, BLANKS first, so its part numbers fit a byte
    offset = 2
    for group in groups:
        parts, size, stats = ct.cut(draft, FRONT, facing, terrain, tiles, group, lofts, colours, sz)
        print(setname, 'vehicle at', min(group), 'parts', len(parts), stats)
        for p in parts:
            # the craft's Chimeras are alike: identical parts are stored once
            key = (tuple(p['sprite']), ct.mcd_record(p, 0, lofts))
            if key not in known:
                known[key] = len(records)
                frames.append(p['sprite'])
                records.append(ct.mcd_record(p, len(records), lofts))
            index = known[key]
            x, y, z, slot = p['from']
            if slot in (ct.WEST, ct.NORTH):
                nslot, nx, ny = ct.turn_wall(slot, x, y, sx, sy, k)
            else:
                nslot, (nx, ny) = slot, ct.turn_tile(x, y, sx, sy, k)
            cell = values.setdefault((nx, ny, z), [0, 0, 0, 0])
            cell[nslot] = offset + index
    pck, tab = ct.encode_pck(frames)
    open(os.path.join(GEN, 'TERRAIN', setname + '.MCD'), 'wb').write(b''.join(records))
    open(os.path.join(GEN, 'TERRAIN', setname + '.PCK'), 'wb').write(pck)
    open(os.path.join(GEN, 'TERRAIN', setname + '.TAB'), 'wb').write(tab)
    msx, msy = (sy, sx) if k % 2 else (sx, sy)
    if any(v > 255 for cell in values.values() for v in cell):
        print('WARNING: part numbers above 255 do not fit a MAP file')
    ct.write_map(os.path.join(GEN, 'MAPS', mapname + '.MAP'), msx, msy, sz, values)
    print(setname, 'parts', len(records), 'hit shapes not in LOFTEMPS (closest used):', lofts.inexact)

# preview the turned craft maps the way the game draws them
xa.SEARCH.insert(0, GEN)
pal = xa.palette()
for facing, draft, setname, mapname in VIEWS:
    full = xa.Terrain(['BLANKS', setname])
    msx, msy, msz, mtiles = xa.load_map(mapname)
    w, h, rgba = xa.crop(*xa.render(full, msx, msy, msz, mtiles, pal))
    xa.write_png(os.path.join(GEN, mapname + '.png'), w, h, rgba, 2)
    print('preview', mapname, msx, msy, msz)
