# Moving Vehicles — Implementation plan

Last updated 2026-10-29 · Aleix Ruiz Sorribes

The [MVP](MVP.md) (Phases 1 to 7) and [M2](M2.md) (Phases 8 to 13) are built in phases on the `moving-vehicles` branch. Each phase builds cleanly and lands as its own commit (pushed to the fork), so the history reads as a series of reviewable steps that can be rolled back one by one. Findings and ideas met on the way go to [NOTES.md](NOTES.md); the in-game checks are collected in [TESTING.md](TESTING.md).

All seven phases are written, built and committed, and the test plan was played on 2026-10-29; the two problems it found are fixed (see Phase 7), and the reopened lines are to play again. Phase 6 was added after the first five, when testing showed that swapping drivers let a vehicle drive without limit, and Phase 7 when we looked at hilly maps, where a vehicle staying on its level drove into hills.

| Phase | Delivers | Playable result | Commit |
| --- | --- | --- | --- |
| 1. Rules | The `battleVehicle` block, read and checked | Mods with the block load; broken blocks are reported | `d94045336` |
| 2. Vehicles exist | Vehicles created from craft maps, kept in saves | Nothing moves yet; vehicles survive save and load | `f39a33a9f` |
| 3. Driving engine | The drive step, route search and the drive state | A crew member on a seat drives by clicking a tile | `587225250` |
| 4. Controls | Drive mode, Drive button and hotkey, route preview and ghost | The full MVP driving experience | `38139c20f` |
| 5. Finish | Messages, demo mods, ruleset reference, test plan | The MVP "Done when" list is ticked | `c9b3a4e4d` |
| 6. Vehicle time units | The vehicle's own `timeUnits` per turn, paid with the driver's | Swapping drivers no longer takes a vehicle further | `296b0782c` |
| 7. Hills | Ground vehicles rest on the highest ground under them and change level as they drive | Vehicles drive up and down hills instead of into them | `1f3e5ed95`, fixes `ec5f200dd` |

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

- [x] Vanilla UFO Defense loads with no new messages (TFTD data isn't installed here; it is checked once before the pitch).
- [x] A test mod with a valid block on the Skyranger (map `PLANE`, 10x20) loads.
- [x] Each broken case (seat outside the map, unknown armor, negative cost, empty rectangles) is reported once, naming the craft.

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
- [x] A battle save from before vehicles (no `vehicles` key) still opens.

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

- Every message ("the way is blocked", "not enough time units" and so on) is a translatable string in `bin/common/Language/OXCE/en-US.yml` and `en-GB.yml`; they were added with the code that uses them, in Phases 3 and 4.
- Validation messages and edge cases found while testing (for example, seats under a spawned turret) are tidied.
- Demo mods in `docs/moving-vehicles/mods/`: a vanilla one on the Skyranger, and the Rosigma Chimera and Baneblade converted from the prototype's test mod. They live outside the engine code, so they are easy to leave out of an upstream proposal.
- `DESIGN.md` gets a ruleset reference section, ready to copy to the OXCE wiki.
- [TESTING.md](TESTING.md) collects every in-game check of the MVP as one plan to tick.
- The MVP "Done when" list is run end to end.

**Checks**

- [x] Both demo mods load with no validation messages (the Rosigma seats are checked against the real Chimera, Steel Legion Chimera and Baneblade maps).
- [ ] Every item of the MVP "Done when" list is ticked.

**Commit:** `Moving vehicles: messages, demo mods and ruleset reference` (one or more commits)

## Phase 6: Vehicle time units

Decided in [GD-16](GAME_DESIGN_DECISIONS.md#gd-16--vehicles-have-their-own-time-units-per-turn): only the driver paid for driving, so crew taking turns at the wheel drove a vehicle without limit.

**Approach**

- `RuleBattleVehicle` reads `timeUnits`; left out, it is 10 straight tiles' worth (`10 × tuPerTile`).
- `BattleVehicle` keeps the time units it has left: full when the battle starts, saved as `timeUnits`, and full again when loading a save made before this phase.
- `SavedBattleGame::endTurn` fills them up for the side whose turn starts, next to where its units get theirs back.
- `VehicleDriveBState` charges each step to both the driver and the vehicle, and stops with `STR_VEHICLE_OUT_OF_TIME_UNITS` when the vehicle can't pay (checked first, since another driver wouldn't help).
- The route preview counts down the lower of the two.
- The step cost stays in one function (`BattleVehicle::getStepCost`), where driver skill ([GD-17](GAME_DESIGN_DECISIONS.md#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further)) will plug in.
- Demo values: Skyranger 40 (10 tiles), Chimeras 48 (12 tiles), Baneblade 48 (8 tiles at 6 per tile).

**Checks**

- [x] Builds with no new warnings.
- [x] Both demo mods load with no messages.
- [x] A battle save made before this phase (vehicles without `timeUnits`) loads.
- [ ] The in-game checks under "Vehicle time units" in [TESTING.md](TESTING.md).

**Commit:** `Moving vehicles: vehicles have their own time units per turn`

## Phase 7: Hills

Decided in [GD-18](GAME_DESIGN_DECISIONS.md#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them): a vehicle staying on its level drove into hills, hiding walkable hill fill and digging through weak hill rock.

**Approach**

- All in `BattleVehicle`, reading terrain as if the vehicle weren't there (`getTerrain`: under its own tiles, the terrain it covers), so planning and driving use the same rules.
- `isGround` tells ground (floors, walkable raised parts, solid parts filling their tile under a floor) from obstacles; `getGroundRaise` and `getGroundHeight` give the ground of a tile and of a column, a filled tile counting as the ground of the level above.
- `findLevel` picks the level for a place: the highest ground under the footprint (each column from the vehicle's lowest tile there), one level per step, nothing two levels below, ground under the middle, tipping at ledges, and never further into higher ground (`countBuried`).
- `checkPlace` judges the terrain the same way and never crushes ground; a vehicle's lowest tiles may stand in hill fill only after coming off a ledge.
- `move` takes the level from `findLevel` and moves passengers, items and route nodes up or down with it; a step whose ground changed since planning is refused with `STR_VEHICLE_TOO_STEEP`.
- The route search plans over tiles and levels, climbing a level costing half as much again; `getStepCost` charges climbing the same way.
- `VehicleDriveBState` moves the camera's view level with the driver when the vehicle changes level, as walking does.
- No ruleset keys and no save changes: a vehicle's position already has its level.

**Checks**

- [x] Builds with no new warnings.
- [x] The rule, simulated on the vanilla mountain and forest blocks (MCD and MAP files read by a script), lets a 3x4 and a 4x6 vehicle climb hills to levels 1 and 2 and come down cliffs, and never strands it where it can't drive back.
- [x] The Rosigma demo mod loads with no messages, and a battle save made before this phase opens (2026-10-29, after the fixes below).
- [ ] The vanilla demo mod loads with no messages (it has no new keys to read; not launched again yet).
- [x] The in-game checks under "Hills" in [TESTING.md](TESTING.md), played 2026-10-29, with the rest of the test plan. Two problems turned up elsewhere in the plan, fixed below.

**Commit:** `Moving vehicles: ground vehicles follow hills` (`1f3e5ed95`)

### Fixes after testing

- The Chimera's hull gun fell out of the vehicle where it hung over lower ground, and was left behind. Mounted guns are now held by their vehicle ([GD-19](GAME_DESIGN_DECISIONS.md#gd-19--mounted-guns-are-held-by-their-vehicle)): `BattleVehicle::isMounted` and `getMountedOn`, asked by `BattleUnit::updateTileFloorState`.
- A place that couldn't take the vehicle always said "The vehicle does not fit there!", and planning knew about units the player couldn't see. The route search now names what blocks the place, and only counts the units its side knows of ([GD-20](GAME_DESIGN_DECISIONS.md#gd-20--the-route-search-only-knows-the-units-its-side-knows)): `checkPlace` takes `knownOnly`, and `STR_VEHICLE_DOES_NOT_FIT_THERE` is gone.

**Checks**

- [x] Builds with no new warnings.
- [x] A battle save made before Phase 7 opens with the Rosigma demo mod, with no messages.
- [x] Builds on Linux (GCC, with OXCE's strict warnings) and 32-bit Windows on GitHub, with no warnings in our code (2026-10-29). The macOS build got no runner and didn't run.
- [ ] The reopened lines in [TESTING.md](TESTING.md) ("Route preview" and "Driving").

**Commit:** `Moving vehicles: fixes from testing the hills phase`

# M2: Combat and AI drivers

[M2](M2.md) continues the phase numbering on the same branch, with the same rules for every phase. Each phase is a series of commits on top of the core that leaves the core's behaviour alone where its keys are left out: a craft without `runOverDamage` is blocked by units as before, a UFO without `battleVehicle` doesn't drive.

| Phase | Delivers | Playable result | Commit |
| --- | --- | --- | --- |
| 8. Running over | `runOverDamage`, `runOverDamageType`; damage, kill credit, ramming | A Chimera runs over the infantry it is driven onto | `377183df4` |
| 9. Reaction fire | Reaction checks for every unit on board after each step | Crew showing on a driving vehicle draw fire | `8264a1c8b` |
| 10. Enemy vehicles | `battleVehicle` under `ufos`, alien-side vehicles, `ufo:` in saves | The traitor convoy's Chimeras are vehicles (not driven yet) | `a7d9f8c3d` |
| 11. AI drivers | Hold, advance or park, once per turn per vehicle | The traitor Chimeras close in on the squad | `13ee31083` |
| 12. Convoy | Settles [GD-25](GAME_DESIGN_DECISIONS.md#gd-25--convoys-drive-for-the-map-edge); `aiExit` and escaping at the map edge | The convoy tries to drive off the map | built last, after 14 |
| 13. Driver skill | Layers 1 and 2 of [GD-17](GAME_DESIGN_DECISIONS.md#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further) | A good driver gets further | `0b374aca1` |
| 14. Enemy drivers | One of the deployed enemies put on each enemy vehicle's seat; `driverRanks`; AI drivers keep their seat | The traitor Chimeras are driven by traitors inside | `da4cbb1eb` |
| 15. Gun crews and hijacking | `gunCrew`; silent guns without a driver; capturing a vehicle and its guns | Killing a driver silences its turrets; taking its seat takes the vehicle | `ae3466baf` |

## Phase 8: Running over

Decided in [GD-21](GAME_DESIGN_DECISIONS.md#gd-21--running-over-is-the-players-choice-ai-drivers-dont-hesitate).

**Approach**

- `RuleBattleVehicle` reads `runOverDamage` (default 0) and `runOverDamageType` (default melee), and links the damage type rule.
- `checkPlace` tells the units in the way apart: the vehicle's own side and other vehicles block; enemies and civilians are run over when `runOverDamage` is above 0, and are handed back as victims instead of blocking. Units already going down (out of health or stunned) no longer count.
- Planning: the player's route search lets the vehicle onto known units only under the clicked place; AI routes (Phase 11) go through known enemies and civilians at a high extra cost.
- `move` refuses a step with victims (`MOVE_RUN_OVER`) without moving anything, so the drive state deals the damage: `BattleUnit::damage` on the side facing the vehicle, with the driver as attacker, then `BattlescapeGame::checkForCasualties` for the kill credit, diaries, score and morale.
- Death animations run after the current state, so the drive state hands the rest of the route to a new drive state queued behind them and ends; the step is taken once the victims are down. If one survives, the step's time units are spent and the drive stops with `STR_VEHICLE_RAMMED`.
- Demo mods: Chimeras and Baneblade (Rosigma) and the Skyranger (vanilla) get `runOverDamage`.

**Checks**

- [x] Builds with no new warnings.
- [ ] Both demo mods load with no messages, and a battle save made before this phase loads (to run when the screen is free).
- [x] The in-game checks under "Running over" in [TESTING.md](TESTING.md). Played 2026-10-30.

**Commit:** `Moving vehicles: vehicles run over units`

## Phase 9: Reaction fire

Decided in [GD-22](GAME_DESIGN_DECISIONS.md#gd-22--reaction-fire-against-everyone-on-board-who-shows).

**Approach**

- After each step, `VehicleDriveBState` asks `TileEngine::checkReactionFire` about every unit on board of the driving side, the driver first, as `UnitWalkBState` does for a walker; a reaction stops the drive.
- Nothing else changes: who sees whom comes from the field of view, already updated after each step, so the hull hides the crew inside.

**Checks**

- [x] Builds with no new warnings.
- [ ] The in-game checks under "Reaction fire" in [TESTING.md](TESTING.md). Played 2026-10-30: roof crew and the closed hull as planned; the turrets were not seen drawing fire (open).

**Commit:** `Moving vehicles: reaction fire against driving vehicles`

## Phase 10: Enemy vehicles

Decided in [GD-23](GAME_DESIGN_DECISIONS.md#gd-23--enemy-vehicles-come-from-ufo-maps).

**Approach**

- `RuleUfo` reads `battleVehicle`, checked against the UFO's map blocks like a craft's.
- `BattlescapeGenerator` runs `BattleVehicle::Placement` around each UFO map it loads, with the UFO's tileset range and the alien side.
- `BattleVehicle` remembers whether it came from a UFO; `getRule` looks in the right list, and saves write `ufo:` instead of `craft:`.
- Rosigma demo: the traitor convoy (`STR_TRAITOR_CHIMERA`) gets a block with its turrets as drivers. Its map holds three Chimeras, not two: the third has no turret, so it can't drive and stays parked.

**Checks**

- [x] Builds with no new warnings.
- [ ] Both demo mods load with no messages, and a battle save made before this phase loads.
- [x] The in-game checks under "Enemy vehicles" in [TESTING.md](TESTING.md). Played 2026-10-30; two lines that later phases changed were rewritten.

**Commit:** `Moving vehicles: UFO maps can be enemy vehicles`

## Phase 11: AI drivers

Decided in [GD-24](GAME_DESIGN_DECISIONS.md#gd-24--ai-drivers-hold-advance-or-stay-parked).

**Approach**

- `RuleBattleVehicle` reads `aiKeepDistance` (default 6).
- The route search gets a second use: every place the vehicle can reach with a given budget of time units, with the cost and crushing of each, sharing the A* code of `findPath`.
- A small new class, `VehicleAI`, decides for a driver: hold, advance (scoring reachable places by distance to the nearest known enemy, then crushing, then cost) or park. Known enemies come from the driver's own AI module (`AIModule::validTarget`).
- `BattlescapeGame::handleAI` asks it as the first action of an AI unit that can drive, once per vehicle per turn, and queues a `VehicleDriveBState` when the vehicle moves.
- The camera follows an AI vehicle while the player can see its driver.
- As built: the route search is shared as `BattleVehicle::search`, with `findPath` (A* to a target) and `findReachable` (every place within a budget) on top; an AI driver spends at most half its own time units driving; `BattleVehicle` keeps a flag (not saved) so each vehicle is decided for once per turn.

**Checks**

- [x] Builds with no new warnings.
- [ ] Both demo mods load with no messages.
- [ ] The in-game checks under "AI drivers" in [TESTING.md](TESTING.md). Played 2026-10-30: all but reaction fire against the enemy Chimera's crew (open, as under "Reaction fire").

**Commit:** `Moving vehicles: AI units drive vehicles`

## Phase 12: Convoy

Settles [GD-25](GAME_DESIGN_DECISIONS.md#gd-25--convoys-drive-for-the-map-edge) first, after the AI drivers of Phase 11 have been played; the approach is written then.

**Commit:** `Moving vehicles: convoys drive for the map edge`

## Phase 13: Driver skill

Layers 1 and 2 of [GD-17](GAME_DESIGN_DECISIONS.md#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further), with the numbers and the rounding settled in [GD-26](GAME_DESIGN_DECISIONS.md#gd-26--how-driving-skill-counts).

**Approach**

- `RuleBattleVehicle` reads `driverReactionsZeroPoint` and `driverReactionsRange`, defaulting to the mod's pilot values.
- `BattleVehicle::getDriverBonus` gives a driver's bonus: soldiers only, and only the craft's pilots when it takes pilots (`Craft::getPilotList`, without changing anything).
- A second `getStepCost` scales a step by the bonus and carries hundredths of a time unit to the next step; the vehicle keeps the carry, saved as `tuCarry` and cleared at the start of each turn.
- `VehicleDriveBState` charges steps with it, and the route preview counts down the same way. AI route planning keeps the plain cost, since AI drivers aren't soldiers.

**Checks**

- [x] Builds with no new warnings.
- [ ] Both demo mods load with no messages, and a battle save made before this phase loads.
- [x] The in-game checks under "Driver skill" in [TESTING.md](TESTING.md). Played 2026-10-30.

**Commit:** `Moving vehicles: skilled drivers get further`

## Phase 14: Enemy drivers

Decided in [GD-27](GAME_DESIGN_DECISIONS.md#gd-27--enemy-vehicles-get-their-driver-from-the-deployed-crew), after the first enemy vehicles (Phase 10) were driven by their turrets. Built before the convoy (Phase 12), which needs it.

**Approach**

- `RuleBattleVehicle` reads `driverRanks`, alien ranks from 0 to 7.
- `BattleVehicle::seatDriver` picks one of the vehicle's side for a seat (dedicated ranks first, then anyone allowed by `drivers` but commanders and leaders; on board first, then the nearest) and moves it there with `SavedBattleGame::setUnitPosition`. `seatDrivers` does it for every vehicle of a side.
- `BattlescapeGenerator` calls it for the alien side right after `deployAliens`, in both places enemies are deployed (a battle and the next stage of a mission).
- `BattlescapeGame::handleAI` turns a driver's walk into no action, so it keeps its seat; vehicles without seats are left alone.
- `VehicleDriveBState` follows an enemy vehicle with the camera while the player sees anyone on board, at the vehicle's middle.
- Rosigma demo: the traitor Chimeras get a seat at the front of each troop bay and no `drivers` list, so any traitor infantry can drive; the turrets only fight.

**Checks**

- [x] Builds with no new warnings.
- [ ] Both demo mods load with no messages, and a battle save made before this phase loads.
- [x] The in-game checks under "Enemy drivers" in [TESTING.md](TESTING.md). Played 2026-10-30.

**Commit:** `Moving vehicles: enemy vehicles get a driver from their crew`

## Phase 15: Gun crews and hijacking

Decided in [GD-28](GAME_DESIGN_DECISIONS.md#gd-28--a-vehicles-guns-need-its-driver-unless-the-vehicle-says-otherwise) and [GD-29](GAME_DESIGN_DECISIONS.md#gd-29--hijacking-take-the-seat-once-the-crew-is-down). Built before the convoy (Phase 12), whose "board the command vehicle" needs a capture rule.

**Approach**

- `RuleBattleVehicle` reads `gunCrew` (`driver` or `none`).
- `BattleVehicle::hasDriver` tells whether anyone on board can drive; `silenceGuns` sets the mounted guns' time units to 0 while they need a driver and have none.
- `BattleVehicle::checkCapture` hands the vehicle to the side whose unit took a seat once its own side on board is down, converts its mounted guns (`BattleUnit::convertToFaction`, as mind control does) and leaves them all without time units; `updateCrews` does both for every vehicle.
- Called after casualties (`BattlescapeGame::checkForCasualties`) and after every walk (`UnitWalkBState::postPathProcedures`), with the messages `STR_VEHICLE_CAPTURED` and `STR_VEHICLE_TAKEN_BY_ENEMY`; `SavedBattleGame::endTurn` silences guns without a driver when each turn starts.
- No save changes: the vehicle's side is already saved, and units save their own side.

**Checks**

- [x] Builds with no new warnings.
- [ ] Both demo mods load with no messages, and a battle save made before this phase loads.
- [ ] The in-game checks under "Gun crews and hijacking" in [TESTING.md](TESTING.md). Played 2026-10-30: the turret went back to the traitors the turn after a capture, fixed below; everything else passed but "your own Chimera without a driver", not played yet.

**Commit:** `Moving vehicles: guns need a driver, and vehicles can be hijacked` (`ae3466baf`)

### Fixes after testing

- A captured turret went back to its old side when the captor's next turn started: `convertToFaction` is mind control, which `BattleUnit::prepareNewTurn` undoes. `BattleVehicle::keepGuns`, called from `SavedBattleGame::endTurn` after the units' turn start, converts the vehicle's mounted guns back and gives them their time units (`BattleUnit::updateUnitStats`). Guns another side controls right now are left alone. ([GD-29](GAME_DESIGN_DECISIONS.md#gd-29--hijacking-take-the-seat-once-the-crew-is-down))
- `STR_VEHICLE_CAPTURED` now says the vehicle is ready next turn.

**Checks**

- [x] Builds with no new warnings.
- [x] Builds on Linux (GCC), macOS (Clang) and 32-bit Windows on GitHub, with no warnings in our code (2026-10-30).
- [x] The captured turret stays yours and fires on the turns after the capture (played 2026-10-30).

**Commit:** `Moving vehicles: a captured vehicle keeps its guns` (`ec84ebb27`)
