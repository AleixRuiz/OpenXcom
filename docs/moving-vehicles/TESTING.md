# Moving Vehicles — MVP test plan

Last updated 2026-10-28 · Aleix Ruiz Sorribes

OXCE has no automated tests (see [NOTES.md](NOTES.md)), so the MVP is checked in two ways: scripted launches for what can be checked without playing, and this in-game plan for the rest. Each line says what to do and what should happen; tick it when it does.

## Setup

1. Build the `moving-vehicles` branch (Release x64).
2. Copy the demo mods from [mods](mods/README.md) into your `mods` folder and enable the one you test.
3. Start a **New Battle** (main menu) with the craft to test: the **Skyranger** with the vanilla mod, the **Chimera** or **Baneblade** with the Rosigma mod. Any ground mission works; a flat terrain makes the first drives easier.

## Scripted checks (no playing needed)

The game takes `-user <folder>` and `-cfg <folder>` on the command line, so a test profile never touches your own saves or mod list, and `-load <save>` opens a save directly.

- [x] Vanilla UFO Defense starts with no new messages.
- [x] A mod with a valid `battleVehicle` block on the Skyranger loads.
- [x] Broken blocks are reported once each, naming the craft: unknown split, negative costs, a piece or a seat outside the map, `split: rects` without pieces, a craft without battle map blocks.
- [x] An unknown driver armor stops loading with "Armor ... not found".
- [x] A battle save without `vehicles` (made by older builds) still opens.

## In battle

**Vehicles are made**

- [ ] The Skyranger is one vehicle; the Chimera craft gives three, one per Chimera.
- [ ] Save the battle and open the save file: a `vehicles:` list with one entry per vehicle. A battle with a craft that has no `battleVehicle` block writes no `vehicles:` key.

**Drive mode**

- [ ] A soldier who is not on a driver seat shows no Drive button, and the drive hotkey (D) does nothing.
- [ ] Walk a soldier onto a driver seat: the Drive button appears at the top right.
- [ ] Press Drive (or D): the button looks pressed. Press it again: it pops back and the soldier walks normally.
- [ ] With drive mode on, select another soldier: drive mode switches off.
- [ ] With the special action buttons full (special weapon, skills and psi), the Drive button can't show, but D still switches drive mode.

**Route preview**

- [ ] With drive mode on, click a tile: arrows mark the route of the vehicle's center, with the time units left after each step, and a see-through vehicle shows where it stops (the soldier's seat ends up on the clicked tile).
- [ ] Steps beyond the driver's or the vehicle's time units (whichever is lower) turn red.
- [ ] Click the same tile again: the vehicle drives there, one tile per step, the camera following.
- [ ] Click another tile instead: a new preview replaces the old one. A right click removes the preview.
- [ ] Click where the vehicle can't fit or can't reach: "The vehicle does not fit there!" or "No route for the vehicle!", and nothing moves.

**Driving**

- [ ] Soldiers inside and on the roof ride along; the turrets on a Chimera ride along.
- [ ] Items lying on the vehicle's floors ride along; a soldier standing outside next to it stays behind.
- [ ] Fences, bushes and other light terrain in the way are crushed and stay destroyed after the vehicle passes.
- [ ] Heavy walls stop the route search, or the drive with "Terrain is in the way!".
- [ ] A unit in the way blocks the vehicle ("A unit is in the way!"); so does another vehicle, and the map edge.
- [ ] Each straight step costs the driver `tuPerTile` time units, each diagonal step half as much again; the drive stops when they run out ("Not Enough Time Units!").

**Vehicle time units**

- [ ] Drive a Chimera as far as it goes (12 straight tiles with the Rosigma demo, 10 for the Skyranger): it stops with "The vehicle can't move any further this turn!", even if the driver has time units left.
- [ ] Put a fresh soldier on the seat: the preview shows no time units left and the vehicle doesn't move.
- [ ] Drive part way with one soldier, swap to another: the second soldier can only use what the vehicle has left.
- [ ] End the turn: on the next turn the vehicle drives its full distance again.
- [ ] Each Chimera of the craft has its own time units: driving one doesn't use up the others'.
- [ ] Save after driving part way and load: the vehicle has the same time units left.
- [ ] The drive stops when a soldier on board spots a new enemy; Shift+click drives on past them.
- [ ] Where the vehicle stood when the battle started, the ground is back after it drives away: no holes in the floor.

**Saves**

- [ ] Drive, save, and load the save: every vehicle, soldier, item and crushed tile is where it was, and the vehicle can keep driving.

**Nothing else changed**

- [ ] Start several Quick Battles in a row, on different missions, with the demo mod on and off: the battle screen opens every time.
- [ ] With drive mode off, soldiers walk, kneel, shoot and open doors as usual, also inside the vehicle.
- [ ] A battle without the demo mods plays exactly as before.
