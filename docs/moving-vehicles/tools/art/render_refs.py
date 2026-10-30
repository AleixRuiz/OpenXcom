"""Renders the Rosigma Chimera maps as references: the player's (facing south) and the traitors' (facing east)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xcomart as xa

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
os.makedirs(OUT, exist_ok=True)
pal = xa.palette()

for name, datasets, mapname in [
    ('chimera_south', ['BLANKS', 'CHIMERA', 'HIDRA', 'LEMON', 'REPULSOR'], 'CHIMERA_SMALL_NON_HYDRA'),
    ('chimera_east_traitor', ['BLANKS', 'CHIMERACHAOS_40k', 'HIDRACHAOS_40k', 'INTERIORCHAOS_40k'], 'CHIMERACONVOY'),
]:
    terrain = xa.Terrain(datasets)
    sx, sy, sz, tiles = xa.load_map(mapname)
    w, h, rgba = xa.render(terrain, sx, sy, sz, tiles, pal)
    w, h, rgba = xa.crop(w, h, rgba)
    xa.write_png(os.path.join(OUT, name + '.png'), w, h, rgba, 2)
    print(name, sx, sy, sz, w, h)
