# Moving Vehicles — MVP implementation plan

Last updated 2026-10-28 · Aleix Ruiz Sorribes

The [MVP](MVP.md) is built in five phases on the `moving-vehicles` branch. Each phase builds cleanly, is tested in game, and lands as its own commit (pushed to the fork), so the history reads as a series of reviewable steps. Findings and ideas met on the way go to [NOTES.md](NOTES.md).

| Phase | Delivers | Playable result |
| --- | --- | --- |
| 1. Rules | The `battleVehicle` block, read and checked | Mods with the block load; broken blocks are reported |
| 2. Vehicles exist | Vehicles created from craft maps, kept in saves | Nothing moves yet; vehicles survive save and load |
| 3. Driving engine | The drive step, route search and the drive state | A crew member on a seat drives by clicking a tile |
| 4. Controls | Drive mode, Drive button and hotkey, route preview and ghost | The full MVP driving experience |
| 5. Finish | Messages, validation polish, demo mods, ruleset reference | The MVP "Done when" list is ticked |

Rules for every phase:

- New code goes in new files; widely included headers (`Tile.h`, `SavedBattleGame.h`, `BattlescapeGame.h`) get forward declarations and as little else as possible, to keep rebuilds small.
- No debug keys, no temporary code left behind at the end of a phase, except the temporary click-to-drive of Phase 3, which Phase 4 replaces.
- Before each commit: a full build with no new warnings, a vanilla battle without vehicles, and the phase's own checks below.
- The prototype (`prototype/moving-vehicles`) is a reference to read, not code to copy: what is reused is rewritten to the new design.

## Phase 1: Rules

**Approach**

- New rule class `RuleBattleVehicle` (`src/Mod/RuleBattleVehicle.h/.cpp`) reading the MVP keys: `split`, `pieces`, `driverSeats`, `drivers`, `tuPerTile`, `crushArmor`. Keys of later milestones are not read yet.
- `RuleCraft` owns an optional `RuleBattleVehicle`, read from `battleVehicle` in `RuleCraft::load` and finished in `RuleCraft::afterLoad`.
- Validation with `Mod::checkForSoftError`, like the rest of `RuleCraft`:
    - each seat and piece rectangle lies inside every map block of the craft's battle terrain;
    - `drivers` names existing armors (linked with `Mod::linkRule`);
    - `tuPerTile` and `crushArmor` are not negative; `split: rects` has at least one rectangle.
- The new files are added to `src/CMakeLists.txt` and the Visual Studio project.

**Checks**

- [ ] Vanilla UFO Defense loads with no new messages (TFTD data isn't installed here; it is checked once before the pitch).
- [ ] A test mod with a valid block on the Skyranger (map `PLANE`, 10x20) loads.
- [ ] Each broken case (seat outside the map, unknown armor, negative cost, empty rectangles) is reported once, naming the craft.

**Commit:** `Moving vehicles: battleVehicle ruleset`

## Phase 2: Vehicles exist

**Approach**

- New class `BattleVehicle` (`src/Savegame/BattleVehicle.h/.cpp`): its craft rule, its tileset range, its layout as loaded (tile offsets and the parts it owns), its position, the terrain each of its tiles covers, and its route nodes.
- Creation in `BattlescapeGenerator::generateMap`, where the craft map is placed: remember the area before `loadMAP`, then record every tile part that came from the craft's tilesets, and split them into vehicles (`connected` or `rects`, `whole` keeps one).
- Missing floors on level 0 under a vehicle count as scorched earth, matching the generator's own fill (see NOTES.md).
- `SavedBattleGame` keeps the list (`std::vector<BattleVehicle*>`), saves it as `vehicles` only when it is not empty, loads it after the tiles, and deletes it with the battle.
- The craft deployment preview creates vehicles too; they are never driven there.

**Checks**

- [ ] The Rosigma Chimera craft gives three vehicles; the Skyranger test mod gives one.
- [ ] Saving and loading keeps the `vehicles` list identical (compared in the save file).
- [ ] A battle without `battleVehicle` writes no `vehicles` key.

**Commit:** `Moving vehicles: create vehicles from craft maps and save them`

## Phase 3: Driving engine

**Approach**

- `BattleVehicle::move` performs one step as the design's drive-step diagram shows: lift the hull, check the new footprint, crush, stamp, carry crew, mounted units, items, route nodes and discovered tiles, then apply gravity to the tiles left behind and above crushed terrain. A refused step puts everything back.
- The footprint check uses a small lookup of the vehicle's own tiles, never a map-sized table, and looks for units only on the tiles it would cover.
- Route search: A* over vehicle positions on its level (straight steps cost 2, diagonal 3, crushing 2 more, no corner cutting), with fit results cached for one search.
- A new battle state `VehicleDriveBState` moves one step per tick: pays `tuPerTile` (x1.5 diagonal), recalculates light and sight, follows with the camera, and stops when out of time units, when the step is refused, or when a crew member spots a new enemy (the walking rule, see NOTES.md).
- Who may drive: a unit of the vehicle's side on one of its seats (or anywhere on board without seats), with its armor in `drivers`, or any unit that can walk when `drivers` is empty.
- **Temporary:** left clicking a tile with a driver selected drives straight there, with no preview. Phase 4 replaces this.

**Checks**

- [ ] The Chimera drives, crushes fences and bushes, and stops at heavy walls, other vehicles and units.
- [ ] Soldiers and items on board arrive with it; a soldier standing outside stays behind.
- [ ] Driving stops on a newly spotted enemy and when time units run out.
- [ ] A battle saved after driving loads with everything where it was.

**Commit:** `Moving vehicles: drive a vehicle one tile per step`

## Phase 4: Controls

**Approach**

- Drive mode lives in `BattlescapeGame`, switched by a **Drive** button in the contextual button row (shown when the selected unit can drive and a slot is free) and by a new hotkey option `keyBattleDrive`. Selecting another unit switches it off.
- With drive mode on, a left click previews the route: the path of the vehicle's center with time units left per step (through the existing path preview markers), and a see-through ghost of the vehicle where it stops, drawn by `Map`. A second click on the same tile drives; Shift+click drives on past newly spotted enemies, as walking does.
- With drive mode off, the unit walks and acts normally.

**Checks**

- [ ] The Drive button appears only for units that can drive, and the hotkey works when the button row is full.
- [ ] The preview shows the right time units, turns red where they run out, and the ghost matches where the vehicle stops.
- [ ] The same soldier can drive, switch drive mode off, and walk out.

**Commit:** `Moving vehicles: drive mode, route preview and ghost`

## Phase 5: Finish

**Approach**

- Every message ("the way is blocked", "not enough time units" and so on) becomes a translatable string in `bin/common/Language/OXCE/en-US.yml` and `en-GB.yml`.
- Validation messages and edge cases found while testing (for example, seats under a spawned turret) are tidied.
- Demo mods in `docs/moving-vehicles/mods/`: a vanilla one on the Skyranger, and the Rosigma Chimera and Baneblade converted from the prototype's test mod. They live outside the engine code, so they are easy to leave out of an upstream proposal.
- `DESIGN.md` gets a ruleset reference section, ready to copy to the OXCE wiki.
- The MVP "Done when" list is run end to end.

**Checks**

- [ ] Every item of the MVP "Done when" list is ticked.

**Commit:** `Moving vehicles: messages, demo mods and ruleset reference` (one or more commits, then the MVP is ready to pitch)
