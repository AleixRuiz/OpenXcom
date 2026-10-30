# Art tools for drawn facings

Python scripts (standard library only) that drew the showcase Chimera's north and west facings ([M3](../../M3.md#art-for-the-showcase), Phase 18). They are working tools, not part of the game: paths point at the author's Rosigma install and are set at the top of `xcomart.py`.

| Script | Does |
| --- | --- |
| `xcomart.py` | Reads X-COM terrain art (MAP, MCD, PCK/TAB, Rosigma's palette) and draws maps as the battlescape does. |
| `render_refs.py` | Renders the existing Chimeras (facing south and east) as references. |
| `template.py` | Builds grey templates of a vehicle in any facing from its tiles' hit shapes (LOFT voxels), so the outline matches the tile grid. |
| `gemini.py` | Calls Google's image models. The API key is read from `~/.gemini-api-key`, sent only in a request header, and never printed. |
| `draft.py` | Asks Nano Banana Pro for a facing: a south template with its finished art as the example, then the template to paint. Counts images against a limit. |
| `evaluate.py` | Shrinks a draft to game scale in the vehicle's own colours and measures how well it fits the template. |
| `cutter.py` | Cuts a painted facing into tile parts: turns the layout and hit shapes, finds which part is in front at each pixel, and writes MCD, PCK/TAB and MAP files. |
| `retry_ground.py` | Asks for the roofless views (the ground level with its roof taken off), retrying while the model is overloaded and falling back to Nano Banana 2. |
| `make_facings.py` | Runs the cutter for the Chimera craft's north and west maps and previews them, roof on and roof off. The roof comes from the full painting, the ground level (hull sides and the inside) from the roofless one, and the floor inside keeps its own art turned round. |

Drafts come back as JPEG; the author converted them to PNG with Windows' `System.Drawing` before `evaluate.py`.
