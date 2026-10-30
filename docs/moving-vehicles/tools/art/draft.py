"""Drafts the Chimera's back views with Nano Banana Pro, within the agreed limit of images."""
import json
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gemini as g

OUT = os.path.join(HERE, 'out')
DRAFTS = os.path.join(HERE, 'drafts')
LOG = os.path.join(DRAFTS, 'log.json')
LIMIT = 30
MODEL = 'gemini-3-pro-image'


def read_png(path):
    data = open(path, 'rb').read()
    pos, w, h, idat = 8, 0, 0, b''
    while pos < len(data):
        n = struct.unpack('>I', data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            w, h = struct.unpack('>II', body[:8])
        elif tag == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * 4
    rows, prev = [], bytearray(stride)
    i = 0
    for _ in range(h):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - 4] if x >= 4 else 0
            b = prev[x]
            c = prev[x - 4] if x >= 4 else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    return w, h, b''.join(rows)


def on_magenta(src, dst, pad=16):
    """Flattens a transparent render onto flat magenta, with a margin, for the model."""
    import xcomart as xa
    w, h, rgba = read_png(src)
    W, H = w + 2 * pad, h + 2 * pad
    out = bytearray(b'\xff\x00\xff\xff' * (W * H))
    for y in range(h):
        for x in range(w):
            k = (y * w + x) * 4
            if rgba[k + 3]:
                d = ((y + pad) * W + x + pad) * 4
                out[d:d + 4] = rgba[k:k + 4]
    xa.write_png(dst, W, H, out)
    return dst


def log():
    return json.load(open(LOG)) if os.path.exists(LOG) else []


def used(entries=None):
    return sum(1 for e in (log() if entries is None else entries) if e['files'])


def draft(name, prompt, images):
    entries = log()
    # only images actually made count (a refused or failed call makes none and costs nothing)
    if used(entries) >= LIMIT:
        print('limit reached, not sending')
        return []
    parts = []
    for path in images:
        parts.append(g.png_part(path))
    parts.append({'text': prompt})
    body_parts = parts
    prefix = os.path.join(DRAFTS, '%02d_%s' % (len(entries) + 1, name))
    status, text, files = g.generate('gemini', MODEL, body_parts, prefix)
    entries.append({'name': name, 'status': status, 'files': [os.path.basename(f) for f in files], 'text': text[:300]})
    json.dump(entries, open(LOG, 'w'), indent=1)
    print(name, status, files, text[:200].replace('\n', ' '))
    return files


STYLE = ('Keep exactly the outline, proportions, size and camera angle of the last grey model, at the same pixel '
         'scale as the finished art (each art pixel is a 4x4 block of screen pixels). Use the same small palette, '
         'flat shading, dark outlines and crisp pixel-art style as the finished green art: no anti-aliasing, no '
         'blur, no gradients, no text, no shadow on the ground. Plain flat magenta background (#FF00FF) everywhere '
         'outside the vehicle. Output one image.')

INTRO = ('You are a pixel artist for the 1994 game X-COM: UFO Defense, painting isometric battlescape vehicles. '
         'Image 1 is a rough grey blocking model of a Warhammer 40,000 Chimera armoured transport, seen from the '
         "game's fixed camera, with its FRONT facing the lower left and its LEFT side facing the lower right. "
         'Image 2 is the finished game art painted over exactly that model: same outline, same size, same camera '
         '(the art adds bevels and sloped armour inside the blocky outline). '
         'Image 3 is the same kind of Chimera in red, front facing the lower right, showing its RIGHT side '
         '(lower left) for reference. ')

NORTH = INTRO + (
    'Image 4 is the grey blocking model of the green Chimera turned around: its REAR faces the lower left and its '
    'RIGHT side faces the lower right. Paint image 4 the way image 2 paints image 1: the green Imperial Guard '
    'Chimera of image 2, seen from behind. The rear shows the open rear door of the troop compartment, with the '
    'assault ramp lowered to the ground and the dark interior inside, between the two track units. The right side '
    '(lower right) carries the same track unit, side panels, hatches and golden Imperial eagle as the left side in '
    'image 2, in the same green, never the red or the chaos symbols of image 3. The round hatch sits on the roof. ' + STYLE)

WEST = INTRO + (
    'Image 4 is the grey blocking model of the green Chimera turned around: its REAR faces the lower right and its '
    'LEFT side faces the lower left. Paint image 4 the way image 2 paints image 1: the green Imperial Guard Chimera '
    'of image 2, seen from behind and from its left. The rear (lower right) shows the open rear door of the troop '
    'compartment, with the assault ramp lowered to the ground and the dark interior inside, between the two track '
    'units. The left side (lower left) is the left side of image 2 seen from the other end: the same track unit, '
    'side panels, hatches and golden Imperial eagle. The round hatch sits on the roof. ' + STYLE)

if __name__ == '__main__':
    os.makedirs(DRAFTS, exist_ok=True)
    flat = {}
    for n in ['template_south', 'chimera_south_one', 'chimera_east_one', 'template_north', 'template_west']:
        flat[n] = on_magenta(os.path.join(OUT, n + '.png'), os.path.join(DRAFTS, 'in_' + n + '.png'))
    which = sys.argv[1:] or ['north', 'west']
    for w in which:
        prompt = NORTH if w == 'north' else WEST
        draft(w, prompt, [flat['template_south'], flat['chimera_south_one'], flat['chimera_east_one'], flat['template_' + w]])
    print('images used:', len(log()), 'of', LIMIT)
