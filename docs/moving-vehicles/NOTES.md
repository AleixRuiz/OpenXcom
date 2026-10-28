# Moving Vehicles — Findings and ideas

A running log of what we learn about the engine while building the feature, and ideas that could make it better. Newest entries at the top of each section; each finding says what it changes in the plan.

## Findings

| Date | Finding | What it changes |
| --- | --- | --- |
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
| Mounted guns (units that can't walk) never drive unless `drivers` names their armor | Avoids a turret on a seat taking the wheel by accident | MVP, Phase 1 |
| Show how far the vehicle can still go (time units / `tuPerTile`) on the Drive button or in the route preview | Players plan drives without trial and error | Phase 4 |
| Shift+click drives on past newly spotted enemies, as it already does for walking (`ignoreSpottedEnemies`) | Same control as walking, no new rule | Phase 4 |
| In OXCE debug mode, outline each vehicle and show its id on the map | Replaces the prototype's debug keys with something useful to testers | After the MVP |
| Check `battleVehicle` blocks in the existing test screen (seats and pieces against each map block) | Modders find broken blocks without starting a battle | After the MVP |
| Smooth motion between tiles (draw the vehicle shifted while it moves) | Driving looks like driving instead of jumping a tile | After the MVP |
| A movement sound per step, set in the ruleset | Vehicles feel heavy | After the MVP |
