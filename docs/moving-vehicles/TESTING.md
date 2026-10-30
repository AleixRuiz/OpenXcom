# Moving Vehicles — MVP test plan

Last updated 2026-10-29 · Aleix Ruiz Sorribes

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

- [x] A soldier who is not on a driver seat shows no Drive button, and the drive hotkey (D) does nothing.
- [x] Walk a soldier onto a driver seat: the Drive button appears at the top right.
- [x] Press Drive (or D): the button looks pressed. Press it again: it pops back and the soldier walks normally.
- [x] With drive mode on, select another soldier: drive mode switches off.
- [x] With the special action buttons full (special weapon, skills and psi), the Drive button can't show, but D still switches drive mode.

**Route preview**

- [x] With drive mode on, click a tile: arrows mark the route of the vehicle's center, with the time units left after each step, and a see-through vehicle shows where it stops (the soldier's seat ends up on the clicked tile).
- [x] Steps beyond the driver's or the vehicle's time units (whichever is lower) turn red.
- [x] Click the same tile again: the vehicle drives there, one tile per step, the camera following.
- [x] Click another tile instead: a new preview replaces the old one. A right click removes the preview.
- [ ] Click where the vehicle can't stand or can't reach: the message says what is in the way ("Terrain is in the way!", "The ground is too steep for the vehicle!", "The vehicle can't leave the map!") or "No route for the vehicle!", and nothing moves.

**Driving**

- [ ] Soldiers inside and on the roof ride along; the turrets on a Chimera ride along, the hull gun at its front too, also where the front hangs over lower ground on a hill.
- [x] Items lying on the vehicle's floors ride along; a soldier standing outside next to it stays behind.
- [x] Fences, bushes and other light terrain in the way are crushed and stay destroyed after the vehicle passes.
- [x] Heavy walls stop the route search, or the drive with "Terrain is in the way!".
- [ ] Click so the vehicle would end on a soldier: "A unit is in the way!"; on another vehicle: "Another vehicle is in the way!". A soldier on the route makes it go around.
- [ ] An enemy the player can't see doesn't change the route: driving into it stops the vehicle with "A unit is in the way!".
- [x] Each straight step costs the driver `tuPerTile` time units, each diagonal step half as much again; the drive stops when they run out ("Not Enough Time Units!").

**Vehicle time units**

- [x] Drive a Chimera as far as it goes (12 straight tiles with the Rosigma demo, 10 for the Skyranger): it stops with "The vehicle can't move any further this turn!", even if the driver has time units left.
- [x] Put a fresh soldier on the seat: the preview shows no time units left and the vehicle doesn't move.
- [x] Drive part way with one soldier, swap to another: the second soldier can only use what the vehicle has left.
- [x] End the turn: on the next turn the vehicle drives its full distance again.
- [x] Each Chimera of the craft has its own time units: driving one doesn't use up the others'.
- [x] Save after driving part way and load: the vehicle has the same time units left.
- [x] The drive stops when a soldier on board spots a new enemy; Shift+click drives on past them.
- [x] Where the vehicle stood when the battle started, the ground is back after it drives away: no holes in the floor.

**Hills**

Use a hilly map: a vanilla mountain or forest Quick Battle, or a Rosigma outdoor map.

- [x] Drive up a gentle hill: the vehicle stays level, the slope disappears under it and comes back after it leaves, and it goes up a level when its front reaches the hilltop. The camera goes up with it.
- [x] The route preview over a hill shows its arrows on the levels the vehicle will be on, and the ghost stands on the hilltop.
- [x] A step that climbs a level costs half as much again (6 instead of 4 for a Chimera); coming down costs the usual.
- [x] A cliff (a whole level up in one tile) can't be climbed: the route goes around it, or "No route for the vehicle!".
- [x] Drive off a one-level ledge: the vehicle comes down when its middle passes the edge, its back end in the ledge until it drives on.
- [x] The vehicle never digs into a hill: mountain rock is never crushed.
- [x] Soldiers on board ride up and down with it. A soldier standing under the raised end of a vehicle stops it from coming down onto them.
- [x] Save on a hilltop and load: the vehicle is still there and drives on.

**Saves**

- [x] Drive, save, and load the save: every vehicle, soldier, item and crushed tile is where it was, and the vehicle can keep driving.

**Nothing else changed**

- [x] Start several Quick Battles in a row, on different missions, with the demo mod on and off: the battle screen opens every time.
- [x] With drive mode off, soldiers walk, kneel, shoot and open doors as usual, also inside the vehicle.
- [x] On flat ground, driving works and costs exactly as before the hills phase.
- [x] A battle without the demo mods plays exactly as before.

## M2

The M2 checks use the same demo mods, updated with each phase.

**Running over** (Phase 8)

- [x] Click so the vehicle ends on an enemy you can see: the route goes there, and the enemy takes the hit when the vehicle reaches it. A weak enemy goes down and the vehicle drives on over the body.
- [x] A strong enemy that survives stops the vehicle: "The vehicle rammed a unit!", the step's time units are spent, and the vehicle stays where it was.
- [x] A kill by running over counts for the driver: it shows in the soldier's diary after the mission.
- [x] Click past an enemy standing in the way (not under the place clicked): the route goes around it, or "No route for the vehicle!".
- [x] Your own soldiers always block: "A unit is in the way!".
- [x] A civilian is only run over when you click so the vehicle ends on them; the civilian loss counts.
- [x] An enemy nobody could see is run over (or rams the vehicle) when the vehicle reaches it.
- [x] A vehicle without `runOverDamage` (remove it from the demo mod) is blocked by every unit, as before.

**Reaction fire** (Phase 9)

- [x] Drive past an enemy with a soldier standing on the roof: the enemy may fire at the soldier, and a shot stops the drive.
- [x] The same drive with everyone inside a closed vehicle (Chimera hatches shut): no reaction fire, the hull hides them.
- [ ] The Chimera's turrets draw reaction fire like the roof crew. (2026-10-30: not seen in play. They are checked like the roof crew, but riders spend no time units, so enemies rarely win the initiative against them; see the note of that date in [NOTES.md](NOTES.md).)
- [x] A drive that draws no fire goes on to its end as before.

**Enemy vehicles** (Phase 10)

Fight the traitor convoy: **New Battle**, the last mission in the list, "Moving vehicles: convoy showcase", with the Chimera as your craft.

- [x] The battle opens; the save file's `vehicles:` list has three entries with `ufo: STR_TRAITOR_CHIMERA` and `faction: 1`, next to the player's own vehicles with `craft:`.
- [x] Nothing else about the battle changes. (This line first said the Chimeras don't move yet; since Phase 11 they drive.)
- [x] Shooting a traitor Chimera damages its hull as before; the turrets fight as before.
- [x] Your soldiers can't drive an enemy vehicle while its side holds it: no Drive button on its seat with traitors still inside. (Since Phase 15 they can once they capture it; see "Gun crews and hijacking".)
- [x] Save and load: the enemy vehicles are still there.

**AI drivers** (Phase 11)

Same traitor convoy battle.

- [x] While the traitors don't know where your squad is, their Chimeras stay parked.
- [x] Once a traitor has spotted your squad, on the aliens' turn a traitor Chimera drives toward it and stops about 6 tiles away, then its turret and crew fire if they can.
- [x] A traitor Chimera whose turret or crew already sees a target doesn't move; they fire.
- [x] The camera follows a traitor Chimera while you can see anyone on board (its turret, say), and the map shows it moving.
- [ ] An enemy Chimera driving past your soldiers draws their reaction fire on whoever shows: its turret, roof crew. (2026-10-30: not sure in play; the same question as the turrets under "Reaction fire".)
- [x] Your soldiers standing in its way are run over or ram it; its own infantry never are.
- [x] Each traitor Chimera drives at most once per aliens' turn, and no further than its time units allow.
- [x] The third Chimera (no turret) drives too, with its own driver (from Phase 14).

**Driver skill** (Phase 13)

- [x] Put a soldier with high reactions (75 or more) on the seat and preview a long drive: the time units left go down more slowly than with a soldier near 55, and the vehicle gets further in a turn.
- [x] A soldier with low reactions (35 or less) gets less far.
- [x] On a craft that takes pilots, a soldier who isn't one of its pilots drives as a 55-reactions soldier would, whatever their reactions.
- [x] Save in the middle of a turn and load: the time units left and the next steps' costs are the same.
- [x] The traitor Chimeras (driven by traitors, who aren't soldiers) go exactly as far as before.

**Enemy drivers** (Phase 14)

- [x] At battle start, each traitor Chimera has a traitor on its driver seat, inside at the front of its troop bay: in a save, a traitor stands on each of `[5, 8, 0]`, `[17, 7, 0]` and `[9, 2, 0]` of the convoy map.
- [x] No leader or commander is put on a seat.
- [x] The battle has as many traitors as before: none are added.
- [x] The traitor on a seat never walks off it; it shoots from there when it can.
- [x] Kill or stun a driver (break in, or a grenade through a hatch): that Chimera stops driving for the rest of the battle.
- [x] A battle save made before this phase opens; nobody is moved in it.

**Gun crews and hijacking** (Phase 15)

In the convoy showcase:

- [x] Kill or stun a traitor Chimera's driver: its turret stops firing at once, and doesn't fire or react in the aliens' next turns.
- [ ] Your own Chimera: with nobody on its driver seat when your turn starts, its turrets have no time units; put a soldier on the seat and they work again from the next turn.
- [ ] On the first turn every turret has its time units, whether or not a soldier starts on its Chimera's seat; walk the driver off the seat and back: the turret keeps its time units. (Reopened 2026-10-30: two of three turrets started silent.)
- [x] Step a soldier onto the seat of a traitor Chimera with infantry still inside: nothing changes.
- [x] Clear the traitors inside (kill or stun them), then step onto the seat: "Vehicle captured! Ready next turn.". The Chimera and its turret are yours, with no time units until your next turn. The traitors may shoot at the turret now: it's yours.
- [x] On your next turn, and the turn after, the turret is still yours: select it and fire it. (Reopened 2026-10-30, when the turret went back to the traitors; played again after the fix the same day.)
- [x] On your next turn, drive the captured Chimera with the soldier on the seat.
- [x] Save and load after a capture: the Chimera and its turret are still yours.
- [x] Set `gunCrew: none` on the traitor convoy in the demo mod: its turrets keep firing after the driver dies.

**Convoy** (Phase 12)

In the convoy showcase (New Battle, last mission, with the Chimera craft):

- [x] The map is farmland. The traitor convoy starts at the west edge, on the middle rows, facing east, with most of its traitors inside and a few on foot; your Chimeras land north of it, in the middle.
- [x] On the aliens' first turn, before they know where you are, the convoy already drives east, the lead Chimera first. None of them stops to fight, and their turrets still shoot at what they see.
- [x] Each Chimera goes about 5 to 7 tiles a turn, around barns and through fences and crops.
- [x] Park one of your Chimeras across its way: the convoy goes around it, or stops as close as it can get.
- [x] A soldier on foot in its way is run over, or rams it to a stop if they survive.
- [x] Kill or stun a convoy Chimera's driver: that Chimera stops for good, and the others drive on.
- [x] Capture a convoy Chimera and drive it to the east edge: it stays.
- [x] Let a convoy Chimera reach the east edge: "An enemy vehicle escaped!", and it disappears with the traitors inside and its turret; the ground under it comes back. A soldier of yours on its roof stays behind, on the ground.
- [x] When every traitor has escaped or is down, the battle ends as usual. The debriefing counts no kill or capture for those who escaped, and nothing they carried.
- [x] Save after an escape and load: the escaped Chimera stays gone.
- [ ] A battle save made before this phase opens.

**Mirror turning** (Phase 16)

With the Rosigma Chimera craft (the convoy showcase works too):

- [ ] Select the soldier on a Chimera's seat and switch drive mode on. Right click a tile east of the Chimera: it turns to face east, mirrored, and the driver and the vehicle pay 8 time units. Right click a tile north of it: it turns back, lined up north to south with its front to the south (it has no art for its back).
- [ ] Right click a tile it already lines up with, ahead of it or behind it: nothing happens.
- [ ] The turned Chimera looks right: hull, walls, ramp, turrets and the hull gun in their places, nothing missing or doubled. Its doors and ramp open and look right.
- [ ] Shots hit the turned hull where it is drawn and pass where it isn't; soldiers inside are covered by its walls as before.
- [ ] Soldiers, turrets and items on board turn with it and stay on board; the soldier on the seat still drives it.
- [ ] A turn without room (a wall or another vehicle where the turned Chimera would stand) is blocked with the reason, and nothing changes. Fences and bushes in the way are crushed, and an enemy in the way is run over.
- [ ] A turn draws reaction fire like a step.
- [ ] With path preview on, a right click removes the preview first, as before; the next right click turns.
- [ ] Drive the turned Chimera: the see-through vehicle of the route preview is drawn turned, and it drives as before.
- [ ] Save with a turned Chimera and load: it is still turned and looks the same.
- [ ] The traitor convoy (facing east) still drives east and never turns: the AI doesn't turn yet.
- [ ] Remove `front` from the Chimera in the demo mod: a right click in drive mode turns the driver, as before.
- [ ] A battle save made before this phase opens.

**Four facings** (Phase 18)

With `facings.rul` and the showcase Chimera's tiles in the Rosigma demo mod, and the Chimera craft:

- [ ] Right click north of a Chimera in drive mode: it turns half round, for 16 time units, and shows its back (open rear door, lowered ramp) and its right side.
- [ ] Right click west of it: it faces west, showing its back and its left side. Right click east: it faces east (its south art mirrored). Right click south: back as it started.
- [ ] In each facing, shots hit the hull where it is drawn, and soldiers walk in and out of the open rear door.
- [ ] Soldiers, the roof turret and the hull gun on board stay on board and turn with it; the soldier on the seat still drives.
- [ ] Turn half round, then drive: the hull gun comes along. (Reopened 2026-10-30: it was left behind.)
- [ ] Lower the camera a level: the troop bay inside looks plausible in each facing (it keeps its old art).
- [ ] Destroy a part of the hull, then turn: it is still gone.
- [ ] Save in each facing and load: same facing, same look.
- [ ] Without the tiles (rename `MV_CHIMERA_N.MCD`): the game starts, the log names the missing map, and the Chimera turns by mirroring.
- [ ] The Steel Legion Chimera, the Baneblade and the traitor convoy still turn by mirroring only.
