"""Grey templates of a vehicle from any facing, built from its tiles' hit shapes (LOFT voxels).

The battlescape projection sends voxel (X, Y, Z) to screen (X - Y, (X + Y) / 2 - Z), so every screen
pixel looks along the voxel direction (1, 1, 1): each pixel is found by walking that line from the
camera side until a solid voxel. Turning the voxel model before rendering shows the vehicle from
behind with exactly the outline and proportions its tiles have.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xcomart as xa

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')


def loftemps():
    data = open(os.path.join(xa.UFO, 'GEODATA', 'LOFTEMPS.DAT'), 'rb').read()
    return struct.unpack('<%dH' % (len(data) // 2), data)


def voxels(terrain, tiles, box):
    """Solid voxels of the tiles inside box = (x0, y0, x1, y1, sz), as a dict-free 3D bytearray."""
    lofts = loftemps()
    x0, y0, x1, y1, sz = box
    wx, wy, wz = (x1 - x0) * 16, (y1 - y0) * 16, sz * 24
    grid = bytearray(wx * wy * wz)
    for (x, y, z), parts in tiles.items():
        if not (x0 <= x < x1 and y0 <= y < y1):
            continue
        for p in range(4):
            got = terrain.part(parts[p])
            if not got:
                continue
            rec = got[2]
            for layer in range(12):
                loft = rec['loft'][layer]
                if loft == 0 or (loft + 1) * 16 > len(lofts):
                    continue
                for vy in range(16):
                    row = lofts[loft * 16 + vy]
                    if not row:
                        continue
                    for vx in range(16):
                        if row & (1 << (15 - vx)):
                            X, Y = (x - x0) * 16 + vx, (y - y0) * 16 + vy
                            for vz in (layer * 2, layer * 2 + 1):
                                Z = z * 24 + vz
                                grid[(Z * wy + Y) * wx + X] = 1
    return wx, wy, wz, grid


def turn(wx, wy, wz, grid, facing_from, facing_to):
    """Turns the model so its front goes from one facing (0, 2, 4, 6) to another, by quarter turns."""
    quarters = ((facing_to - facing_from) % 8) // 2
    for _ in range(quarters):
        # a quarter turn clockwise seen from above: north becomes east, (X, Y) -> (wy - 1 - Y, X)
        nwx, nwy = wy, wx
        out = bytearray(len(grid))
        for Z in range(wz):
            for Y in range(wy):
                for X in range(wx):
                    if grid[(Z * wy + Y) * wx + X]:
                        out[(Z * nwy + X) * nwx + (wy - 1 - Y)] = 1
        wx, wy, grid = nwx, nwy, out
    return wx, wy, wz, grid


def render(wx, wy, wz, grid):
    """Grey render: top faces light, south faces mid, east faces dark, as the game lights them."""
    solid = lambda X, Y, Z: 0 <= X < wx and 0 <= Y < wy and 0 <= Z < wz and grid[(Z * wy + Y) * wx + X]
    # screen u = X - Y in [-(wy - 1), wx - 1]; v = (X + Y) // 2 - Z
    u0, v0 = -(wy - 1), -wz
    width, height = wx + wy, (wx + wy) // 2 + wz + 2
    rgba = bytearray(width * height * 4)
    for v in range(v0, v0 + height):
        for u in range(u0, u0 + width):
            # walk from the camera side: the largest Z whose voxel can project here first
            for Z in range(wz - 1, -1, -1):
                X = (u + 2 * (v + Z)) // 2
                Y = X - u
                if X >= wx or Y >= wy:
                    continue
                if X < 0 or Y < 0:
                    break
                if grid[(Z * wy + Y) * wx + X]:
                    if not solid(X, Y, Z + 1):
                        shade = 200
                    elif not solid(X, Y + 1, Z):
                        shade = 150
                    else:
                        shade = 110
                    # darker where the tile grid runs, so the tiles show
                    if X % 16 == 0 or Y % 16 == 0 or Z % 24 == 0:
                        shade -= 25
                    k = ((v - v0) * width + (u - u0)) * 4
                    rgba[k:k + 4] = bytes((shade, shade, shade, 255))
                    break
    return width, height, rgba


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    terrain = xa.Terrain(['BLANKS', 'CHIMERA', 'HIDRA', 'LEMON', 'REPULSOR'])
    sx, sy, sz, tiles = xa.load_map('CHIMERA_SMALL_NON_HYDRA')
    box = (0, 1, 4, 8, sz)  # the first Chimera
    model = voxels(terrain, tiles, box)
    for name, facing in [('south', 4), ('east', 2), ('north', 0), ('west', 6)]:
        wx, wy, wz, grid = turn(*model, 4, facing)
        w, h, rgba = render(wx, wy, wz, grid)
        w, h, rgba = xa.crop(w, h, rgba)
        xa.write_png(os.path.join(OUT, 'template_%s.png' % name), w, h, rgba, 4)
        print(name, w, h)
