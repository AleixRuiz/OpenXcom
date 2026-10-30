"""Drafts the roofless (ground level) views, retrying while the model is overloaded."""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import draft as d

D = d.DRAFTS
INTRO = ('You are a pixel artist for the 1994 game X-COM: UFO Defense, painting isometric battlescape vehicles. '
         'Image 1 is a rough grey blocking model of the GROUND LEVEL of a Warhammer 40,000 Chimera armoured transport '
         'with its ROOF TAKEN OFF, front facing the lower left. Image 2 is the finished game art of exactly that: the '
         'vehicle with its roof removed, so you look down into the troop compartment between the two track units. ')
VIEW = {
    'north': ('Image 3 is the finished art of the whole green Chimera seen from behind, roof on: its REAR faces the lower '
              'left and its RIGHT side faces the lower right. Image 4 is the grey blocking model of the ground level of '
              'that vehicle, seen the same way, roof taken off. '),
    'west': ('Image 3 is the finished art of the whole green Chimera seen from behind, roof on: its REAR faces the lower '
             'right and its LEFT side faces the lower left. Image 4 is the grey blocking model of the ground level of '
             'that vehicle, seen the same way, roof taken off. '),
}
TASK = ('Paint image 4: the Chimera of image 3 with its roof taken off, the way image 2 shows the vehicle of image 1. '
        'The two track units and the outer sides look exactly as in image 3, cut off flat at the top where the roof was. '
        'Between them you look down into the troop compartment as in image 2: the grey metal floor, the inner walls with '
        'the same blue windows and benches as image 2, the open rear door with the ramp lowered to the ground, and the '
        "driver's compartment at the front end. ")

todo = sys.argv[1:] or ['north', 'west']
for attempt in range(16):
    # Nano Banana Pro first; if it stays overloaded for half an hour, Nano Banana 2
    d.MODEL = 'gemini-3-pro-image' if attempt < 10 else 'gemini-3.1-flash-image'
    for n in list(todo):
        ins = [os.path.join(D, 'in_template_south_ground.png'), os.path.join(D, 'in_chimera_south_ground.png'),
               os.path.join(D, 'in_full_%s.png' % n), os.path.join(D, 'in_template_%s_ground.png' % n)]
        if d.draft(n + '_ground', INTRO + VIEW[n] + TASK + d.STYLE, ins):
            todo.remove(n)
    if not todo:
        break
    time.sleep(180)
print('still to do:', todo, '| images made:', d.used(), 'of', d.LIMIT)
