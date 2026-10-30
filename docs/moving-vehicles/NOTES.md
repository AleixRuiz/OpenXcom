# Moving Vehicles — Findings and ideas

A running log of what we learn about the engine while building the feature, and ideas that could make it better. Newest entries at the top of each section; each finding says what it changes in the plan. Game design choices and their reasons go to [GAME_DESIGN_DECISIONS.md](GAME_DESIGN_DECISIONS.md).

## How OXCE is tested and reviewed

- **No unit tests.** The repository has no test framework or test target. The CI workflow (`.github/workflows/nightly.test`, run by hand or on the `test` branch) only compiles: Windows with MSVC, and Linux with GCC using `-Wall -Wextra -Wshadow -Wsuggest-override` and more. The in-game test screen (`Menu/TestState`) checks mod data (route nodes, MCD files, palettes, script tags, map resources), not code.
- **How we test instead:** rule problems are caught when the mod loads (`checkForSoftError`, `linkRule`); scripted launches with an isolated profile (`-user`, `-cfg`, and `-load <save>` to open a save directly) check that mods load and saves open; each phase has an in-game checklist. Adding a test framework to OXCE itself is not proposed: the project is conservative about new infrastructure.
- **Contributions:** OXCE accepts no pull request without a forum discussion first (suggestion board, one feature per thread, describe the problem as well as the solution). Modding features are accepted only from modders, meaning established ones or those who published something non-trivial. The pull request template asks for a separate branch, one feature per request, the coding conventions, and a detailed explanation.

## Conventions we follow

From the [OpenXcom coding style](https://www.ufopaedia.org/index.php/Coding_Style_(OpenXcom)), `.editorconfig`, `.clang-format` and the code around ours:

- Tabs for C++ indentation, Allman braces, braces even around one-line blocks, an empty line at the end of every file, no trailing whitespace; 2 spaces in `.rul` and `.yml` files.
- `CamelCase` names: classes start uppercase, methods and variables lowercase, private members start with `_`, enums and constants are `UPPER_CASE`.
- Comments with `//`. Every class has a `/** */` description in its header; every method has a one-line `///` brief in the header and a `/** */` description in its definition, with `@param` and `@return`, in full sentences ending with a period.
- `*` and `&` next to the name (`int *a`, `const std::string &s`); objects passed by pointer or const reference; const getters; `++i`; `nullptr`, never `NULL`; no null check before `delete`.
- One class per header and source file, named after the class; useless includes removed.
- The style page still says C++03, but the project builds as C++17 and current OXCE code uses `auto`, range `for` loops and lambdas, so we follow the code around ours where the page is out of date.
- Code must also compile cleanly with GCC and Clang (Linux CI), not only MSVC.
- New source files are listed in `src/CMakeLists.txt` and the Visual Studio project and filters; `.vcxproj` files keep Windows line endings.

## Findings

| Date | Finding | What it changes |
| --- | --- | --- |
| 2026-10-30 | An enemy can only react to a unit whose reaction score (reactions x time units left / maximum time units) is no higher than its own (`TileEngine::getSpottingUnits` and `getReactor`). A walker's score drops with every step it pays for; units riding a vehicle pay nothing, so they keep their full score for the whole drive. | Turrets and other riders draw reaction fire far less often than walkers do; in play the Chimera turrets were not seen drawing any. A question for GD-22. |
| 2026-10-30 | `BattleUnit::convertToFaction` is mind control: when the controlling side's turn starts, `BattleUnit::prepareNewTurn` puts the unit back on its original side, with no time units. The original side is what the debriefing counts by (an original player unit without a soldier is taken for a tank and recovered as an item). | Captured guns keep their original side and are converted back at each turn start of their vehicle's side (`BattleVehicle::keepGuns`). |
| 2026-10-29 | An `alienDeployments` entry without `width` and `length` gets a map of size 0 (`AlienDeployment::getDimensions` copies them as they are), unless its map script resizes the map. Nothing fits on it, and the game stops with "Craft ... could not be placed on the map". Rosigma's convoy deployment sets only `height` because the 40k master already sets 50 x 50. | A new demo deployment always sets its own size. |
| 2026-10-29 | OXCE's build workflow is stored as `.github/workflows/nightly.test`, a name GitHub never runs. A copy named `nightly.yml` on a `test` branch of the fork builds on Windows (MSVC, 32-bit), Linux (GCC with `-Wall -Wextra -Wshadow -Wsuggest-override`, set in `src/CMakeLists.txt`) and macOS (Clang); macOS runners are sometimes not available. | Before each pitch-ready milestone, push `moving-vehicles` onto `test` and check the Linux log for warnings in our files. |
| 2026-10-29 | A unit falls when anything checks gravity on its tile (`TileEngine::applyGravity`) and `BattleUnit::updateTileFloorState` finds no floor under it; `setTile` updates that state on every move. Rosigma's Chimera hull gun stands in the front plate, a vehicle tile without a floor of its own, so it fell a level where the vehicle hung over lower ground, and was left behind. | `updateTileFloorState` asks `BattleVehicle::getMountedOn`: a mounted gun is held by its vehicle (GD-19). |
| 2026-10-29 | Planning a walk ignores units the player can't see (`Pathfinding::isBlocked`: the walker's own side, and for the player the units on screen); the walk stops when it bumps into one. | The vehicle route search uses the same rule (GD-20). |
| 2026-10-29 | A test profile in a deep folder can't open Rosigma's files: their full paths pass the 260-character limit, and OXCE then crashes while loading sprites. | Test profiles are reached through a short junction. |
| 2026-10-29 | A simulation of a movement rule over real map blocks (a script reading the MCD and MAP files) shows where a vehicle of a given size can drive and whether it can always drive back, before anyone plays. | Used to check GD-18 on the vanilla mountain and forest blocks. A scratch tool, not part of the repository. |
| 2026-10-29 | A vehicle takes the object slot of every tile it covers, so raised ground under it was already hidden (an earlier note here said slopes showed through its floor; that was wrong). Hills were the problem. OXCE builds them from parts filling a tile up to the next level (terrain level -24) under the hilltop's floor: walkable in the forest blocks (hidden like a slope, so the vehicle drove into the hill) and solid rock in the mountain blocks (big wall, armor 50, crushed by a Chimera, so it dug a tunnel). Vanilla hills rise a level over 3 or 4 tiles. Units step up at most 8 voxels and go up a level on ground raised 16 or more (`Pathfinding::getTUCost`). The terrain files hold about 190 raised walkable parts in vanilla (13 sets) and 3,400 in Rosigma. | Ground vehicles follow the ground, with hill fill as ground that is never crushed: GD-18, Phase 7. |
| 2026-10-28 | `BattlescapeGame`'s constructor checks for casualties, which updates the battle screen's action buttons before `BattlescapeState` has stored the new `BattlescapeGame`, and that pointer was left uninitialized until then. Our Drive button read it there, so the game crashed when a battle opened, or not, depending on leftover memory, with or without vehicles. | `_battleGame` starts as `nullptr` and the button update checks it. Battle screen code reached from button updates must not assume the battle game exists yet. |
| 2026-10-28 | On Windows, a memory access crash on the main thread never reaches OXCE's crash handler: the C runtime's exception filter ends the process with exit code 3, with no log line, no crash dump and no Windows error report. | A game that silently disappears is a crash, not a mod problem. A small debugger script (start the game as a debuggee, log exceptions and exit calls with stacks from the PDB) found this one in a single run. |
| 2026-10-28 | Units get their time units back in `SavedBattleGame::endTurn`, for the side whose turn starts, after the side has changed. | Vehicles are filled up in the same place, so they follow exactly the units' turn order. |
| 2026-10-28 | A finished battle state's `action.result` is shown by `BattlescapeGame::popState` as a translated warning to the player. | The drive state only sets a string ID when it stops; no message code of its own. |
| 2026-10-28 | A hidden surface ignores every event, key presses included (`InteractiveSurface::handle`). | The drive hotkey is handled in `BattlescapeState::handle`, so it works when the Drive button has no free slot. |
| 2026-10-28 | `State::add` builds a battle button's pressed picture before the button's icon is drawn. | The Drive button calls `initSurfaces()` again after drawing its icon. |
| 2026-10-28 | Vanilla `SPICONS.DAT` has only two frames (launch and psi). | The Drive icon is drawn in code from the psi frame's colors, in the original 3x5 lettering. |
| 2026-10-28 | Options are declared in `Options.inc.h`, which nearly every file includes. | Adding the drive hotkey rebuilt the whole game; later option changes should be batched together. |
| 2026-10-28 | The Skyranger's troop compartment is level 1 of its map (it stands on landing gear, the ramp leads down). | Vanilla seats can sit on level 1; the demo's seat is `[4, 9, 1]`. |
| 2026-10-28 | A map block's height is only known once its MAP file is read (`MapBlock` defaults to 4 levels until then). | Seats are checked for x and y when the mod loads, and for their level when the battle starts. |
| 2026-10-28 | Placing a craft or UFO part on a tile also removes the terrain object there (`BattlescapeGenerator::loadMAP`). | Every vehicle tile owns its object slot, so the terrain object comes back when the vehicle leaves. |
| 2026-10-28 | The game takes `-user`, `-cfg` and `-load <save>` on the command line. | Scripted test launches use their own profile and never touch the player's saves or mod list. |
| 2026-10-28 | OXCE does not tag releases; the dev branch is `oxce-plus`. 8.7.1 is commit `441cae1b0`, and the newest dev commit (`f0134ef80`) only adds a CI file on top of it. | Our branch starts from `f0134ef80`; we merge `upstream/oxce-plus` when it moves. |
| 2026-10-28 | A craft map is loaded with its own copy of each of its tilesets, appended after the terrain's (`BattlescapeGenerator::generateMap`), so every craft tile part has a tileset index in the craft's own range. Saves store tilesets by name, in order, so the range survives save and load. | Vehicle parts are told apart from terrain by tileset range; no ownership data needed on tiles for craft vehicles. |
| 2026-10-28 | Right after placing the craft, the generator fills every empty floor on level 0 with scorched earth. | A vehicle that leaves a level 0 tile must put scorched earth back where there was no floor, as the prototype did. |
| 2026-10-28 | Walking stops on a new enemy by comparing the walker's own "spotted this turn" list before and after each step (`UnitWalkBState`). | Driving uses the same rule over the driver and the crew on board, instead of the prototype's count of all visible enemies. |
| 2026-10-28 | The contextual buttons at the top right (special weapon, skills, psi) have only 3 slots (`SPECIAL_BUTTONS_MAX`). | The Drive button takes a slot when free; the hotkey always works. Raising the limit to 4 is an option to discuss. |
| 2026-10-28 | Every soldier reports the unit type `SOLDIER` in battle, whatever its soldier type. | `drivers` lists armor types, which every unit has (soldiers, tanks, aliens). |
| 2026-10-28 | Engine strings live in `bin/common/Language/OXCE/`, one file per language; other languages are updated upstream in bulk. | We add new strings to `en-US.yml` and `en-GB.yml` only. |
| 2026-10-28 | Battle options and hotkeys are registered as `OptionInfo` entries (`OPTION_OXCE`, section `STR_BATTLESCAPE`) and appear in the controls screen by themselves. | The drive hotkey is one new option, no menu code. |
| 2026-10-28 | The craft deployment preview builds the same battle map. | Creating vehicles must be harmless in the preview; nothing there can drive. |
| 2026-10-28 | Map scripts can place the craft at a raised level (`_craftZ`). | Vehicle positions always include the level the craft was placed at. |
| 2026-10-27 | Changing a header included almost everywhere (`Tile.h`, `SavedBattleGame.h`, `BattlescapeGame.h`) rebuilds most of the game. A fully parallel build runs the compiler out of memory, and after such a failed build MSBuild may skip files that need recompiling, linking old and new code together. | Keep MVP code in new files and touch widely included headers as little as possible (forward declarations). After a failed build, rebuild with `-m:1 -p:CL_MPCount=2`. |

## Ideas

| Idea | Why it could help | When |
| --- | --- | --- |
| A ruleset key for the Drive button's picture (a `SPICONS.DAT` frame), like special weapons have | Mods can give the button art that fits them | After the MVP |
| Show the drive hotkey in the Drive button's tooltip | Players learn the hotkey without opening the options | After the MVP |
| Mounted guns (units that can't walk) never drive unless `drivers` names their armor | Avoids a turret on a seat taking the wheel by accident | Done in Phase 1 (`RuleBattleVehicle::canDrive`) |
| Show how far the vehicle can still go (time units / `tuPerTile`) on the Drive button or in the route preview | Players plan drives without trial and error | After the MVP; the route preview already shows the time units left per step |
| Shift+click drives on past newly spotted enemies, as it already does for walking (`ignoreSpottedEnemies`) | Same control as walking, no new rule | Done in Phase 4 |
| In OXCE debug mode, outline each vehicle and show its id on the map | Replaces the prototype's debug keys with something useful to testers | After the MVP |
| Check `battleVehicle` blocks in the existing test screen (seats and pieces against each map block) | Modders find broken blocks without starting a battle | After the MVP |
| Smooth motion between tiles (draw the vehicle shifted while it moves) | Driving looks like driving instead of jumping a tile | After the MVP |
| A movement sound per step, set in the ruleset | Vehicles feel heavy | After the MVP |
