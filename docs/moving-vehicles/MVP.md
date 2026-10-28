# Moving Vehicles — MVP

Last updated 2026-10-28 · Aleix Ruiz Sorribes

The MVP is milestone M1 of the [design](DESIGN.md#roadmap): the player drives craft maps as vehicles, with the crew and cargo on board, through terrain that blocks or is crushed, and the battle saves and loads with them. It is the first thing we pitch to OXCE, so it must be small, opt-in and clean enough to review.

## What the player can do

- Deploy with a craft whose map is marked as a vehicle; each vehicle of a convoy craft (like Rosigma's three Chimeras) is a separate vehicle.
- Select a crew member standing on a driver seat and switch on drive mode.
- Click a tile to preview the vehicle's route: the path of its center, the time units left after each step, and a see-through ghost where it stops.
- Click the same tile again to drive there, one tile per step, with the camera following.
- Switch drive mode off and walk the same soldier out of the vehicle.
- Drive through light terrain (fences, bushes, weak walls), which is crushed and stays destroyed.
- Save in the middle of the battle and load it again with every vehicle where it was.

## MVP features

| Feature | Scope in the MVP | Design section |
| --- | --- | --- |
| Ruleset | `battleVehicle` block under `crafts`, with `split`, `pieces`, `driverSeats`, `drivers`, `tuPerTile`, `crushArmor`; checked when the mod loads | Ruleset design |
| Vehicles from craft maps | Craft map split into vehicles by connected tiles or by rectangles, at battle start | Core model |
| What moves | Hull parts (with damage), units on its floors, mounted units, items on its floors, route nodes, discovered tiles | Core model |
| Driving | A crew member on a seat drives in drive mode; pays `tuPerTile` per tile (x1.5 diagonal) | Driving and controls |
| Route preview | Center path, time units left per step, ghost at the end, second click confirms | Driving and controls |
| Movement rules | Map edge, other vehicles and all units block; solid terrain up to `crushArmor` is crushed; special terrain blocks; no corner cutting; gravity on tiles left behind and above crushed terrain | Movement rules |
| Interruptions | Driving stops when out of time units, when a new enemy comes into view, or when the next step is blocked | Driving and controls |
| Saves | `vehicles` list in the battle save; saves without it load unchanged | Saves, compatibility and performance |
| Messages | Every message through translatable strings (English first) | Where we are |
| Performance | No map-sized allocations or scans of every unit per step or per route check | Saves, compatibility and performance |

## Not in the MVP

| Left out | Comes in | Why it waits |
| --- | --- | --- |
| Running over units (units only block) | M2 | Needs damage, kill credit and morale rules |
| Reaction fire against a driving vehicle | M2 | The rule needs agreement with OXCE first |
| UFO vehicles and AI drivers | M2 | A UFO vehicle is pointless until the AI can drive it |
| Turning | M3 | Needs the mirrored-tile engine change, which OXCE should agree to first |
| Flying | M3 | Not needed to prove the core |
| Choosing where drop pods land | Side track after M1 | Its own feature, pitched separately |
| Showcase missions, mounted gun lists, extraction and the other ideas | Later | Built on top of M2 and M3 |

## Decisions taken for the MVP

- **Craft maps only.** UFO vehicles join in M2 together with AI drivers. The ruleset block is the same, so nothing changes for modders later.
- **No spawned driver.** A crew member drives; a craft without a seat lets anyone on board drive.
- **Units always block.** Running over comes in M2, so an MVP vehicle never hurts a unit.
- **No debug shortcuts.** Testing uses the demo mods below, not debug keys.

## Demo mods

- **Vanilla:** a small mod that marks a vanilla UFO Defense craft map as a vehicle, to show the feature is not Rosigma-specific.
- **Rosigma:** the Chimera and Baneblade converted from the prototype's test mod to the `battleVehicle` keys.

## Done when

- [ ] The game builds with no new warnings on Windows, and the CMake build still lists every new file.
- [ ] Vanilla UFO and TFTD battles with no `battleVehicle` play and save exactly as before.
- [ ] The vanilla demo craft and the Rosigma Chimera both drive, crush light terrain and are blocked by heavy terrain and units.
- [ ] Soldiers ride along, walk out in drive mode off, and items left on board ride along too.
- [ ] A battle saved mid-drive and loaded again keeps every vehicle, crew member, item and crushed tile where it was.
- [ ] A broken `battleVehicle` block (seat outside the map, unknown driver armor) is reported when the mod loads.
- [ ] Every new ruleset key is described in the design document, ready for the OXCE wiki.
