# Moving Vehicles — Game design decisions

Last updated 2026-10-29 · Aleix Ruiz Sorribes

Why moving vehicles plays the way it does. Each entry records a game design choice, why we made it, what we turned down, and what would make us look at it again, so later revisions and forum discussions start from the reasons and not only from the result.

[DESIGN.md](DESIGN.md) says how the feature works today; this log says why. Engine findings and implementation ideas go to [NOTES.md](NOTES.md) instead.

**How to add an entry.** Give it the next number and a status. Never delete an entry: when a decision changes, mark the old one *Replaced by GD-nn* and write the new one.

| Status | Meaning |
| --- | --- |
| Decided | Agreed and built, or agreed for the milestone it names |
| Proposed | Recommended, waiting for agreement |
| Replaced | Overruled by a later entry |

## Index

| ID | Decision | Status | Milestone |
| --- | --- | --- | --- |
| [GD-01](#gd-01--a-vehicle-is-moving-terrain-not-a-big-unit) | A vehicle is moving terrain, not a big unit | Decided | M1 |
| [GD-02](#gd-02--a-crew-member-drives-no-spawned-driver) | A crew member drives; no spawned driver | Decided | M1 |
| [GD-03](#gd-03--driving-from-a-seat-with-a-drive-mode) | Driving from a seat, with a drive mode | Decided | M1 |
| [GD-04](#gd-04--who-may-drive-is-set-by-armor) | Who may drive is set by armor | Decided | M1 |
| [GD-05](#gd-05--driving-costs-the-drivers-time-units) | Driving costs the driver's time units | Decided, extended by GD-16 | M1 |
| [GD-06](#gd-06--diagonal-steps-cost-more-and-cant-cut-corners) | Diagonal steps cost more and can't cut corners | Decided | M1 |
| [GD-07](#gd-07--units-always-block-in-the-mvp) | Units always block in the MVP | Decided | M1 |
| [GD-08](#gd-08--light-terrain-is-crushed-special-terrain-always-blocks) | Light terrain is crushed, special terrain always blocks | Decided | M1 |
| [GD-09](#gd-09--driving-stops-when-a-new-enemy-is-seen) | Driving stops when a new enemy is seen | Decided | M1 |
| [GD-10](#gd-10--vehicles-never-push-or-crush-each-other) | Vehicles never push or crush each other | Decided | M1 |
| [GD-11](#gd-11--ground-vehicles-stay-on-their-level) | Ground vehicles stay on their level | Replaced by GD-18 | M1 |
| [GD-12](#gd-12--no-vehicle-health) | No vehicle health | Decided | M1 |
| [GD-13](#gd-13--craft-maps-first-ufo-vehicles-with-ai-drivers) | Craft maps first, UFO vehicles with AI drivers | Decided | M1, M2 |
| [GD-14](#gd-14--ai-drivers-follow-the-players-rules) | AI drivers follow the player's rules | Decided | M2 |
| [GD-15](#gd-15--turning-only-to-facings-the-art-can-show) | Turning only to facings the art can show | Decided | M3 |
| [GD-16](#gd-16--vehicles-have-their-own-time-units-per-turn) | Vehicles have their own time units per turn | Decided | M1 |
| [GD-17](#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further) | Skilled and dedicated drivers make a vehicle go further | Direction agreed; layers 1 and 2 decided in GD-26 | M2 and later |
| [GD-18](#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them) | Ground vehicles rest on the highest ground under them | Decided | M1, Phase 7 |
| [GD-19](#gd-19--mounted-guns-are-held-by-their-vehicle) | Mounted guns are held by their vehicle | Decided | M1, Phase 7 fixes |
| [GD-20](#gd-20--the-route-search-only-knows-the-units-its-side-knows) | The route search only knows the units its side knows | Decided | M1, Phase 7 fixes |
| [GD-21](#gd-21--running-over-is-the-players-choice-ai-drivers-dont-hesitate) | Running over is the player's choice, AI drivers don't hesitate | Decided | M2, Phase 8 |
| [GD-22](#gd-22--reaction-fire-against-everyone-on-board-who-shows) | Reaction fire against everyone on board who shows | Decided | M2, Phase 9 |
| [GD-23](#gd-23--enemy-vehicles-come-from-ufo-maps) | Enemy vehicles come from UFO maps | Decided | M2, Phase 10 |
| [GD-24](#gd-24--ai-drivers-hold-advance-or-stay-parked) | AI drivers hold, advance or stay parked | Decided | M2, Phase 11 |
| [GD-25](#gd-25--convoys-drive-for-the-map-edge) | Convoys drive for the map edge | Decided | M2, Phase 12 |
| [GD-26](#gd-26--how-driving-skill-counts) | How driving skill counts | Decided | M2, Phase 13 |
| [GD-27](#gd-27--enemy-vehicles-get-their-driver-from-the-deployed-crew) | Enemy vehicles get their driver from the deployed crew | Decided | M2, Phase 14 |
| [GD-28](#gd-28--a-vehicles-guns-need-its-driver-unless-the-vehicle-says-otherwise) | A vehicle's guns need its driver, unless the vehicle says otherwise | Decided | M2, Phase 15 |
| [GD-29](#gd-29--hijacking-take-the-seat-once-the-crew-is-down) | Hijacking: take the seat once the crew is down | Decided | M2, Phase 15 |
| [GD-30](#gd-30--turning-is-a-step-and-mirroring-is-free-art) | Turning is a step, and mirroring is free art | Decided | M3, Phase 16 |
| [GD-31](#gd-31--drives-turn-the-vehicle-toward-where-it-goes) | Drives turn the vehicle toward where it goes | Decided | M3, Phase 17 |
| [GD-32](#gd-32--up-to-four-drawn-facings-mirroring-fills-the-gaps) | Up to four drawn facings, mirroring fills the gaps | Decided | M3, Phase 18 |

## Decisions

### GD-01 · A vehicle is moving terrain, not a big unit

Decided · 2026-10-28

**Decision.** A vehicle is a set of map tile parts that the engine lifts and puts down elsewhere, carrying the units and items on it. The crew are ordinary units standing on it.

**Why.** It works with the craft map art mods already have, with no new sprites. Soldiers walk in and out, take cover inside, shoot out of it and blow holes in it, all with existing rules.

**Turned down.** One huge multi-tile unit (a "titan"): it needs art for every facing and animation, and it is a different feature. Parked.

**Revisit when.** A mod wants a vehicle that is mostly a weapon and not a place to stand in.

### GD-02 · A crew member drives; no spawned driver

Decided · 2026-10-28

**Decision.** One of the units on board drives. The engine never spawns a driver unit.

**Why.** The prototype's spawned skull on the roof was shot, counted in the score and saved like a soldier. A real crew member makes driving a choice with a cost: the soldier who drives is not shooting, and losing them stops the vehicle.

**Turned down.** A spawned driver unit (the prototype); the vehicle acting on its own, with no driver.

### GD-03 · Driving from a seat, with a drive mode

Decided · 2026-10-28

**Decision.** A unit drives only from one of the vehicle's `driverSeats`, or from anywhere on board if it has none. The player switches drive mode on to drive and off to walk.

**Why.** Where the crew stands matters: someone has to be in the cab. The same soldier can drive, then climb out and fight.

**Turned down.** Any unit on board drives; a separate "vehicle" selection that is not a unit.

### GD-04 · Who may drive is set by armor

Decided · 2026-10-28

**Decision.** `drivers` lists armor types. Without it, any unit of the vehicle's side that can walk drives. Units that can't walk (mounted guns) drive only when their armor is listed.

**Why.** Every unit has an armor: soldiers, tanks and aliens. In battle every soldier reports the same unit type, so unit types can't tell a pilot from a rifleman. Keeping mounted guns out by default stops a turret on a seat from taking the wheel by accident.

**Turned down.** Unit types (all soldiers are the same); soldier types (aliens and tanks have none).

**Revisit when.** Driver skill ([GD-17](#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further)) needs soldier types or craft pilots as well.

### GD-05 · Driving costs the driver's time units

Decided · 2026-10-28

**Decision.** The driver pays `tuPerTile` for each tile the vehicle moves.

**Why.** It uses the same economy as walking, so players already understand it: a soldier who drives far has nothing left to shoot with.

**Turned down.** Free movement once per turn (what the prototype's AI vehicles did).

**Extended by** [GD-16](#gd-16--vehicles-have-their-own-time-units-per-turn): the driver still pays, and the vehicle pays the same from its own time units.

### GD-06 · Diagonal steps cost more and can't cut corners

Decided · 2026-10-28

**Decision.** A diagonal step costs 1.5 times a straight one, and the vehicle must also fit on both straight steps around it.

**Why.** It matches the extra distance. A hull can't slip through a gap between two corners that it could not drive straight through.

### GD-07 · Units always block in the MVP

Decided · 2026-10-28

**Decision.** In the MVP, any unit in the way stops the vehicle. Running over enemies comes in M2 with `runOverDamage`.

**Why.** Running over needs rules for damage, kill credit, score and morale that should be agreed first. An MVP vehicle never hurts anyone.

### GD-08 · Light terrain is crushed, special terrain always blocks

Decided · 2026-10-28

**Decision.** Solid tile parts with armor up to `crushArmor` are destroyed when the vehicle drives through them, and stay destroyed. Indestructible (armor 255), explosive, lift, exit, start and mission objective parts always block.

**Why.** Driving through fences and bushes is the point of a tank. Special parts would break missions: driving through an objective, setting off explosives by touch, or removing an exit.

### GD-09 · Driving stops when a new enemy is seen

Decided · 2026-10-28

**Decision.** A drive stops when the driver or any crew member on board sees an enemy they had not seen this turn. Shift + click drives on anyway, as it does for walking.

**Why.** It is the walking rule players know. The crew are the vehicle's eyes, so all of them count.

### GD-10 · Vehicles never push or crush each other

Decided · 2026-10-28

**Decision.** Another vehicle in the way always blocks.

**Why.** Merging or pushing hulls would need a rule for whose tiles win. Ramming between vehicles can be its own follow-up.

### GD-11 · Ground vehicles stay on their level

*Replaced by [GD-18](#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them)* · 2026-10-29 · decided 2026-10-28

**Decision.** A ground vehicle never changes level; slopes and stairs are out of scope. Only flying vehicles (M3) climb or descend.

**Why.** Craft maps are flat, and a vehicle that tilts has no art.

**Replaced.** Hilly maps change level all the time, and a vehicle that stays on its level drives into hills. [GD-18](#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them) lets ground vehicles follow the ground.

### GD-12 · No vehicle health

Decided · 2026-10-28

**Decision.** A vehicle is exactly as broken as its tiles. With no eligible driver on board, it can't move.

**Why.** The hull is terrain, which already takes damage. A second health bar would disagree with what the player sees on the map.

**Revisit when.** We add `criticalParts` (an engine block whose destruction disables the vehicle).

### GD-13 · Craft maps first, UFO vehicles with AI drivers

Decided · 2026-10-28

**Decision.** The MVP drives craft maps only. UFO vehicles come in M2 together with AI drivers, using the same `battleVehicle` block.

**Why.** An enemy vehicle is pointless until the AI can drive it, and a smaller first pitch is easier for OXCE to accept.

*2026-10-29:* the pitch now waits for M2 and M3 ([upstream strategy](DESIGN.md#upstream-strategy)), so the second reason no longer applies; the first still holds, and the core is still offered first.

### GD-14 · AI drivers follow the player's rules

Decided · 2026-10-28

**Decision.** AI drivers sit on a seat, pay time units, and only know the enemies their side has seen.

**Why.** The prototype's AI vehicles moved for free once per turn toward player units they could not see. That is cheating, and players notice it.

### GD-15 · Turning only to facings the art can show

Decided · 2026-10-28

**Decision.** Without new art, a vehicle turns by mirroring between the two facings its art shows, south and east. Other facings need per-facing maps from the mod. The prototype's facing arrow is not part of the engine.

**Why.** The battle camera looks from the south-east, so craft art never shows the back of a vehicle. Mirroring it the other way would show sides that were never drawn.

### GD-16 · Vehicles have their own time units per turn

Decided · 2026-10-28 · built in Phase 6

**Problem.** Only the driver pays for driving. With several crew who can drive, the player drives, swaps seats and drives again: a vehicle with three crew goes three soldiers' worth of tiles in one turn.

**Decision.**

- New key `timeUnits` under `battleVehicle`: the vehicle's time units per turn. It starts the battle full and fills up again at the start of its side's turn, when units get theirs back.
- Each step costs the same from the vehicle and from the driver, and needs both to have enough.
- Swapping drivers still works, but only within the vehicle's limit, so a fresh crew member can take over from a tired one.
- The route preview shows the lower of the two, and the stop message says which one ran out.
- The step cost is worked out in one place, from the vehicle and its driver, so driver skill ([GD-17](#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further)) can change it later.
- The turning cost key reserved for M3 is renamed from `tuPerTurn` to `tuPerQuarterTurn`, so it is not read as "per battle turn".

**Why.** The limit belongs to the machine (fuel, engine), not to whoever sits in the cab. Making the vehicle pay as well caps its range without banning a normal handover between crew members.

**Turned down.**

- One driver per turn: the vehicle's range would depend on the soldier's stats, a killed driver would strand it until next turn, and a normal handover would be blocked.
- A time unit cost for taking the seat: makes the trick dearer but leaves the range unlimited with enough crew.
- The vehicle pays and the driver doesn't: the driver would cross the map and still shoot with all their time units.

**Settled with it.**

- Left out, `timeUnits` is 10 straight tiles' worth (10 × `tuPerTile`), so no vehicle drives without a limit by accident. Turned down: no limit by default, which keeps the exploit for every mod that forgets the key.
- Demo values: Skyranger 40 (10 tiles), Chimera 48 (12 tiles), Baneblade 48 (8 tiles at 6 per tile).
- When both run out, the vehicle's message wins, since another driver wouldn't help.

### GD-17 · Skilled and dedicated drivers make a vehicle go further

Direction agreed · 2026-10-28 · not for the MVP

**Problem.** Any crew member drives equally well, so there is no reason to keep a dedicated driver, while Rosigma and other mods already have pilot soldier types (`STR_ADEPTAS_PILOT`) and OXCE already rewards good pilots in air combat.

**What OXCE already does.** In air combat, each craft's assigned pilots (the Pilots screen, `pilots` on the craft, `allowPiloting` on the soldier type) improve it from their averaged stats: firing gives an accuracy bonus, reactions a dodge bonus, bravery the approach speed. The bonus is `(stat − zero point) × range / 100`, with the zero point and range set in the mod (`pilotReactionsZeroPoint: 55`, `pilotReactionsRange: 60` by default).

**Proposal**, in layers, cheapest first:

1. **Skill makes each tile cheaper.** The driver's reactions (the stat OXCE already uses for pilots' dodge) scale the step cost from [GD-16](#gd-16--vehicles-have-their-own-time-units-per-turn) with the same formula as air combat: a good driver gets more tiles out of the same vehicle time units, a poor one fewer. Each vehicle sets its own zero point and range. Pilot soldier types are rewarded through the stats their type gives them, with no special case in the engine.
2. **Assigned pilots count fully.** If the craft has pilot slots, the soldiers assigned in its Pilots screen drive with their full skill and anyone else drives as a novice, with no bonus. This reuses OXCE's existing pilot assignment, so keeping a dedicated driver becomes a decision made at the base.
3. **Driving trains the driver.** A unit that drives during a turn gains reactions experience once that turn, so the regular driver improves over a campaign. Once per turn keeps players from farming it by driving back and forth.
4. **Recognition.** Tiles driven are counted in the soldier diary, so mods can award commendations, and commendations can already give soldier bonuses that raise the stat.
5. **Script hook.** For mods that want their own rule (a soldier type, a tag, another stat), a script hook that returns the step cost from the vehicle and the driver.

**Why.** It builds on what OXCE players and modders already know from air combat instead of a new stat. The vehicle's time units stay the hard limit, so a better driver means a better use of the vehicle, never unlimited range. Swapping to the best driver mid-turn helps only for what is left of the turn.

**Turned down for now.**

- A new "driving" stat: OXCE's soldier stats are fixed, and adding one touches the whole game. Script tags cover mods that really want one.
- A bigger vehicle time unit pool for a good driver: who drives can change during the turn, so the pool would have to be recalculated mid-turn.
- Skill that lets a driver ignore the stop on a newly seen enemy: Shift already does that, for everyone.

**Agreed.**

- Layers 1 and 2 (skill makes each tile cheaper, assigned pilots count fully) are planned for M2; layers 3 to 5 come later.
- Training from driving may also improve reaction fire: it is the same stat, and quick reflexes fit both.

**Open.** The size of the bonus range, and each vehicle's zero point; set when layer 1 is built and tried in play.

### GD-18 · Ground vehicles rest on the highest ground under them

Decided · 2026-10-29 · built in Phase 7 · replaces [GD-11](#gd-11--ground-vehicles-stay-on-their-level)

**Problem.** OXCE builds a hill from raised ground (terrain parts with a negative terrain level, rising 4 to 8 voxels per tile; a level is 24) up to a hilltop: the floor of the level above, resting on parts that fill their tile up to it. In vanilla that fill is solid rock in the mountain blocks and ground units walk onto in the forest blocks. Hills rise a level over 3 or 4 tiles, and some mountain blocks go up to level 2, with cliffs. Units take it one tile at a time: they step up at most 8 voxels and move up a level once the ground under them is raised 16 or more. A vehicle can't: it is a flat, rigid block several tiles long that always sits on a whole level, because its art can't tilt, so on a slope it covers ground of different heights, and on a terraced hill ground of two or three levels.

Under GD-11 a vehicle stayed on its level. Raised ground under it was already hidden, since a vehicle takes the object slot of every tile it covers, but it drove into hills: walkable hill fill was hidden like a slope, and solid hill rock was crushed when weak enough (vanilla mountain rock has armor 50, the Rosigma Chimera crushes up to 100), digging a tunnel under the hilltop. Raised ground is common: the terrain files have about 190 raised parts units walk on in 13 vanilla sets, and about 3,400 in Rosigma.

**Decision.** The vehicle rests on the highest ground under it, like a box on an uneven floor.

- **What is ground.** Floors; raised parts units walk on (slopes, forest hills); and solid parts filling their tile up to the next level with a floor on top (mountain rock). A part that fills its tile is the ground of the level above. Other solid parts (crates, sandbags, walls) stay obstacles, crushed or blocking as before ([GD-08](#gd-08--light-terrain-is-crushed-special-terrain-always-blocks)). Ground is never crushed.
- **Its level.** For each column of its footprint, take the ground under the vehicle's lowest tile there, and put the vehicle on the level of the highest. Raised ground on its own level is hidden under it while it stands there and comes back when it leaves; lower ground is bridged, as under a landed Skyranger's body. Measuring under the lowest tile means a Skyranger stands on its landing gear, not on its belly.
- **One level per step**, and no part of it over ground two levels or more below it.
- **Its middle needs ground.** The ground under its middle must be less than a level below it, so it can't climb where its middle would hang, and a long vehicle can't climb a hill that rises a level in less than about half its length.
- **Coming off a ledge.** Where its middle would hang, it tips over instead and comes down a level, its back end standing in the ledge (hidden under it). It can drive on away from the ledge but never further into higher ground, so it can't climb a cliff that way. It can also come down slopes it couldn't climb.
- **Climbing costs more.** A step that climbs a level costs half as much again; a diagonal climb costs 2.25 times `tuPerTile`. Coming down costs the usual.
- **Driving it.** The route search plans over tiles and levels, the level of each step following from the ground. The preview and the ghost show the levels, and the camera goes up and down a level with the vehicle, as it does with a walking unit. If the ground has changed since the route was planned, the step is refused with "The ground is too steep for the vehicle!".

**Why.** A vehicle resting on the highest ground only stands in higher ground where it came off a ledge, so it climbs hills instead of digging into them, and never hides another level's floor or the units on it while driving uphill. One rule covers bumps, slopes, ramps and terraces, the vehicle stays level as its art requires, and players can predict it: a vehicle gets onto a hill when its front does. A simulation of the rule on the vanilla mountain and forest blocks showed a 3x4 and a 4x6 vehicle climbing mountain hills to level 2 and forest hills to level 1, coming down cliffs, and able to drive back from every place they reached.

**Turned down.**

- Keep GD-11 and make raised ground block: vehicles would be stuck on most outdoor maps, Rosigma's above all.
- Crush raised ground flat: it changes the map for good, and many slopes are too strong anyway (much of Rosigma's raised ground has armor 100 to 255).
- Sit on the ground under its middle, as units do: on a ramp of two or three tiles its front would be buried in the hilltop, hiding the next level's floor and any units on it.
- Tilting vehicles, or a map per slope: art nobody has.
- Vehicles in sections that follow the ground separately: a vehicle is one map with an interior, and splitting it breaks the interior.

**Settled with it.**

- Climbing a level costs 1.5 times a normal step; coming down costs the same as on flat ground.
- The middle needs ground less than a level below it.
- A vehicle may drive off a one-level ledge (it tips), but never climbs one.
- Solid parts filling a tile with nothing on top (sandbags, crates) stay obstacles, so a sandbag line is crushed, not climbed.
- Parts filling a tile that units walk onto are ground even when they stand alone (a crate soldiers climb), so a vehicle that can't climb onto one is blocked rather than crushing it.

**Revisit when.** Rosigma maps show lone walkable crates blocking vehicles where players expect them crushed.

### GD-19 · Mounted guns are held by their vehicle

Decided · 2026-10-29 · built in the Phase 7 fixes

**Problem.** A unit that can't walk (a mounted gun, such as a Rosigma turret) rides along on any of the vehicle's tiles, floor or not. The Rosigma Chimera's hull gun stands in its front plate, a vehicle tile with no floor of its own, above the terrain's floor. Since [GD-18](#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them) a vehicle may hang over lower ground; where its front did, OXCE found nothing under the gun and dropped it a level, and the vehicle drove on without it (found in testing on an Ork outpost, where the Chimeras stand on level 1).

**Decision.** A unit that can't walk, with all its tiles on a vehicle's tiles, is mounted on the vehicle: it rides along and never falls, wherever the vehicle stands. Units that walk ride only on the vehicle's floors and fall where there is no floor, as anywhere else.

**Why.** A mounted gun is part of the vehicle, and craft maps place hull guns on tiles without a floor on purpose. Giving those tiles a floor would change the mod's art and let soldiers walk out onto the front plate.

**Turned down.**

- Not letting a vehicle hang where a mounted gun has no floor under it: vehicles would stop at the edge of every hill for an art detail.
- Moving hull guns onto floor tiles: needs changes in every mod, and the guns would sit in the wrong place.

**Settled with it.** A mounted gun stays held even when the vehicle tile it stands on is destroyed: the vehicle is still there ([GD-12](#gd-12--no-vehicle-health)).

### GD-20 · The route search only knows the units its side knows

Decided · 2026-10-29 · built in the Phase 7 fixes · extends [GD-07](#gd-07--units-always-block-in-the-mvp)

**Problem.** Planning a route checked every unit on the map, seen or not: it went around enemies the player hadn't spotted and refused places where one stood, telling the player where hidden enemies were. And a place the vehicle couldn't go to always gave "The vehicle does not fit there!", whatever was in the way, so "A unit is in the way!" only ever showed during a drive (found in testing).

**Decision.**

- The route search goes around the units its side knows of: its own units and, for the player, every unit on screen. This is the rule OXCE uses to plan a walk. Any other unit in the way stops the drive when the vehicle reaches it, with "A unit is in the way!", as bumping into one stops a walk.
- When the clicked place can't take the vehicle, the message says why: a unit, another vehicle, the map edge, terrain, or ground too steep. What stands in the way is named before the ground.
- Units still always block ([GD-07](#gd-07--units-always-block-in-the-mvp)).

**Why.** Planning must not reveal what the player can't see, and following the walking rule means players already know what to expect. A specific message tells the player what to do about it: move the soldier, or pick another place.

**Turned down.**

- Every unit blocks the plan, as before: it gives hidden enemies away.
- One message for every reason: the player can't tell a soldier in the way from a wall.

**Settled with it.** Until AI drivers come in M2, vehicles of the other sides plan knowing every unit.

### GD-21 · Running over is the player's choice, AI drivers don't hesitate

Decided · 2026-10-29 · M2, Phase 8 · extends [GD-07](#gd-07--units-always-block-in-the-mvp)

**Problem.** In the MVP every unit blocks a vehicle, so a tank is no threat to infantry in its way, and one soldier standing on a road stops a convoy.

**Decision.**

- `runOverDamage` (default 0: units block, as in the MVP) and `runOverDamageType` (default melee). A step onto an enemy or a civilian deals that damage once to each unit hit, on its armor side facing the vehicle, credited to the driver: a kill counts for the soldier's diary, the score and civilian losses like any other.
- If every unit hit is killed or knocked out, the vehicle drives on. If one survives, the vehicle has rammed it: it stays where it was, the step's time units are spent, and the drive stops with "The vehicle rammed a unit!".
- Units of the driver's side and other vehicles always block. A large unit takes the damage once.
- **The player's route** goes around every unit the player knows of, except the units under the place clicked: running over is always a choice, never a side effect of a route.
- **AI drivers' routes** go through the enemies and civilians they know of, at a high extra cost: around when that's cheap, over when that's the way.
- A unit the driver's side didn't know about is run over, or rams the vehicle, when the vehicle reaches it.

**Why.** The driver getting the kill was settled in the design. Paying for a ram makes it a choice with a cost. The player's route rule avoids running over civilians by accident; AI drivers driving through units make standing in a convoy's road dangerous, which the convoy showcase needs.

**Turned down.**

- Damage to the vehicle from ramming: there is no vehicle health ([GD-12](#gd-12--no-vehicle-health)); the hull is damaged by weapons.
- Pushing units aside: OXCE has no pushed movement, and where to push to is often unclear (walls, other units, edges).
- Letting the player's route run over units on the way: civilians would die to a careless click, and the score with them.

**Settled with it.** Items lying where the vehicle arrives, bodies of units it ran over among them, end up on its floor and ride along, as any item on a tile it drives onto.

### GD-22 · Reaction fire against everyone on board who shows

Decided · 2026-10-29 · M2, Phase 9

**Problem.** A driving vehicle carries its crew past enemies without any reaction fire, so it is a safe way across open ground.

**Decision.** Each step counts as a step for every unit on board. After it, enemies who see a unit on board may react to it, exactly as to a walking unit (OXCE's reaction rules and scores, unchanged): the driver first, then the others. Any reaction shot stops the drive, as it stops a walk. The hull blocks sight like any wall, so a closed vehicle draws fire only to what shows: roof crew, turrets, open hatches. The same applies to AI vehicles, with the player's units reacting.

**Why.** It reuses the walking rule players know, and makes the hull worth having as cover, which is what an armoured transport is for.

**Turned down.**

- Checking only the driver: a squad on an open roof would cross a killing field untouched.
- Reaction fire against the hull: the hull is terrain, and units only react to units.

**Settled with it.** A vehicle with several crew showing draws more reaction checks per step than a lone walker: it is a bigger target.

### GD-23 · Enemy vehicles come from UFO maps

Decided · 2026-10-29 · M2, Phase 10 · carries out [GD-13](#gd-13--craft-maps-first-ufo-vehicles-with-ai-drivers)

**Decision.** The same `battleVehicle` block under `ufos` turns the UFO's map into vehicles of the alien side, by the same rules as crafts: tile parts from the UFO's tilesets, split into vehicles, seats and drivers, with mounted guns driving when their armor is in `drivers`. Every UFO map placed in the battle (a map script can place several) gives its own vehicles. Their time units fill at the start of the aliens' turn. Saves record `ufo:` instead of `craft:`, so a craft and a UFO with the same name can't be confused.

**Why.** One block for crafts and UFOs was settled in the design. Rosigma's traitor convoy is a UFO whose map holds three Chimeras, so enemy vehicles need no new map art.

**Turned down.** Vehicles inside ordinary terrain map blocks: it needs ownership of single tile parts (the hijack idea), and UFO maps already cover the showcase.

### GD-24 · AI drivers hold, advance or stay parked

Decided · 2026-10-29 · M2, Phase 11 · follows [GD-14](#gd-14--ai-drivers-follow-the-players-rules)

**Decision.**

- Once per turn, the first AI unit able to drive a vehicle (on a seat, allowed by `drivers`, with time units) decides for it, as its first action; then it and the crew act as usual.
- **Hold:** if any unit on board sees a target it would attack (the OXCE AI's own rule), the vehicle stays and the crew fights.
- **Advance:** otherwise, toward the nearest enemy the AI knows of, to the place reachable this turn whose middle is closest to `aiKeepDistance` tiles from it (default 6), preferring places that crush less. Known means what the OXCE AI already knows: enemies spotted recently enough for the unit's intelligence.
- **Park:** with no known enemy, the vehicle stays where it is.
- Routes follow [GD-21](#gd-21--running-over-is-the-players-choice-ai-drivers-dont-hesitate): through enemies and civilians when that's the way, never through their own side, never crushing special terrain ([GD-08](#gd-08--light-terrain-is-crushed-special-terrain-always-blocks)).

**Why.** Same rules, time units and knowledge as the player and as other AI units ([GD-14](#gd-14--ai-drivers-follow-the-players-rules)). Holding while the crew has targets keeps turret vehicles shooting instead of wandering off, and keeping a distance stops them driving into the player's squad.

**Turned down.**

- Free moves once per turn toward the player, as in the prototype: cheating.
- Patrolling along route nodes with no known enemy, as the design first said: landed alien vehicles driving around at random look wrong. Convoys get their own goal ([GD-25](#gd-25--convoys-drive-for-the-map-edge)).

**Settled with it.** An AI driver drives with at most half its own time units, keeping the rest to fight; the vehicle's time units still limit the drive.

**Revisit when.** AI vehicles feel too passive or too reckless in play; the distance and the hold rule are the knobs.

### GD-25 · Convoys drive for the map edge

Decided · 2026-10-30 · M2, Phase 12 · proposed 2026-10-29, settled once the AI drivers had been played

**Decision.**

- **`aiExit`** (north, east, south or west) makes a vehicle of an AI side a convoy, driving for that edge of the map.
- **It never stops to fight.** On every turn of its side it drives toward its edge as far as its time units go, whatever its crew sees and whether it knows of an enemy or not. Its turrets and crew keep shooting from on board. Like any AI driver it takes at most half its driver's time units ([GD-24](#gd-24--ai-drivers-hold-advance-or-stay-parked)), so it goes about half as far per turn as a player's vehicle and can be caught.
- **Its way:** the cheapest way to the nearest place at its edge, preferring ways that crush less. It runs over infantry in the way ([GD-21](#gd-21--running-over-is-the-players-choice-ai-drivers-dont-hesitate)), goes around buildings and vehicles, and gets as close as it can when boxed in. A convoy drives its lead vehicle, the one nearest its edge, first, so the others don't run into it.
- **Escaping:** a convoy vehicle that touches its edge, with someone on board able to drive it, leaves the battle as soon as its side acts next.
    - The units of its side on board, turrets and stunned crew included, and the items on board leave with it. They count as neither killed nor captured, and the items are lost.
    - Anyone else on board stays where they were and comes down to the ground.
    - The player sees "An enemy vehicle escaped!".
- **For the mission, nothing more (option A).** The battle goes on; if no enemies are left on the map it ends as usual, and the player scores only what was killed, stunned or captured.
- A convoy vehicle the player has captured never escapes, wherever it is driven.
- **How the player stops it:**
    - Park a vehicle in its way: vehicles block vehicles.
    - Kill or stun its driver ([GD-27](#gd-27--enemy-vehicles-get-their-driver-from-the-deployed-crew), [GD-28](#gd-28--a-vehicles-guns-need-its-driver-unless-the-vehicle-says-otherwise)).
    - Board it and take its seat ([GD-29](#gd-29--hijacking-take-the-seat-once-the-crew-is-down)).
    - Soldiers on foot don't block it: they are run over, unless they survive and ram it to a stop.

**Why.** "Stop them before they get away" is a goal players read at once, and it needs no new mission rules in OXCE. An escape reuses the time out of units left behind between mission stages, which the debriefing already counts as neither killed nor captured. A convoy that stopped to fight could be held forever by a soldier standing in view. Half speed keeps it catchable: a player's Chimera goes about twice as far in a turn, and its passengers ride without spending time units.

**Turned down.**

- A penalty in the debriefing for each enemy that escaped (B), or a mission lost when a convoy vehicle escapes (C): stronger stakes, but new mission rules to ask OXCE for. Revisit if escapes feel free in play.
- Holding to fight like other AI vehicles: see above.
- Taking the player's soldiers on board away with it, missing in action: losing soldiers to a map edge without a fight would feel unfair.
- An exit set by the mission (`alienDeployments`) rather than the vehicle: the art fixes which way a vehicle faces, and vehicles can't turn before M3. Revisit with turning.

**Later.**

- Extra loot for each convoy vehicle the player stops, beyond what its crew carries (the user's idea, 2026-10-30).
- Damage when vehicles collide, and a command vehicle as the mission's objective.

### GD-26 · How driving skill counts

Decided · 2026-10-29 · M2, Phase 13 · settles layers 1 and 2 of [GD-17](#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further)

**Decision.**

- **The bonus** is the air combat pilot formula: (driver's reactions − zero point) × range / 100 percent, capped at 50 either way. Each step's cost (diagonal and climbing included) is that much cheaper, or dearer when the bonus is negative, for both the driver and the vehicle.
- **The numbers** default to the mod's pilot values (`pilotReactionsZeroPoint` 55, `pilotReactionsRange` 60), so a driver with 75 reactions gets 12% further and one with 35 gets 12% less far. A vehicle can set its own with `driverReactionsZeroPoint` and `driverReactionsRange`; a range of 0 turns skill off.
- **Fractions of a time unit carry over** to the next step, within the turn, and are saved: with steps of 4 time units, a 12% bonus saves a time unit about every other step instead of never.
- **Only soldiers have driving skill.** Aliens, tanks and mounted guns drive with no bonus, so AI vehicles go exactly as far as their ruleset says.
- **Assigned pilots.** If the vehicle's craft takes pilots, only its pilots (as OXCE picks them for air combat: those assigned in the Pilots screen, or the qualified crew when there are just enough) drive with their skill; any other soldier drives as a novice, with no bonus and no penalty. A craft without pilot slots lets every soldier drive with their skill.
- The route preview counts down the same costs.

**Why.** Reusing the pilot formula and numbers means nothing new for players or modders to learn (GD-17). Carrying fractions is what makes the usual small bonuses count at all with steps of a few time units. Keeping skill to soldiers leaves AI vehicles predictable for modders.

**Turned down.**

- Rounding each step's cost: bonuses under about 12% would never change anything with 4 time unit steps, which is most soldiers.
- A malus for non-pilots in a craft with pilot slots: being a novice is penalty enough, and a malus would make swapping seats feel punishing.

**Revisit when.** Play shows the bonus too small or too big to notice; the default range and the cap are the knobs (GD-17's open question).

### GD-27 · Enemy vehicles get their driver from the deployed crew

Decided · 2026-10-29 · M2, Phase 14 · follows [GD-02](#gd-02--a-crew-member-drives-no-spawned-driver) and [GD-14](#gd-14--ai-drivers-follow-the-players-rules)

**Problem.** OXCE places enemies on the UFO map's route nodes by rank, and many start outside the UFO, so nothing puts anyone on a driver seat: on the Rosigma convoy map, 2 of the 9 spawn nodes are inside a Chimera and none is on a seat. Making the turret the driver, as the first demo did, needs no seats, but the vehicle stops for good as soon as that exposed turret dies, and it isn't how the player drives.

**Decision.**

- When the battle starts, once the enemies are deployed, each enemy vehicle with driver seats and nobody on them who can drive gets one of its side's deployed units moved onto a seat. No unit is added: how many enemies there are, of which kind and with what gear, doesn't change.
- **Dedicated drivers:** `driverRanks` lists the alien ranks put on the seat first, for example 4 to make a UFO's navigators its drivers.
- Otherwise any unit allowed by `drivers` takes the seat, except commanders and leaders, who ride as passengers; units already on board come first, then the nearest. Large units don't take seats.
- If nobody can, the vehicle stays parked.
- **An AI driver keeps its seat:** it fights from there and never walks off it.
- Vehicles without seats keep the old rule: any unit on board allowed by `drivers` drives, a turret for instance.
- The camera follows an enemy vehicle while the player sees anyone on board, since its driver now sits behind the hull.

**Why.** No map editing, so it works for any mod's UFOs as they are. The driver is a real member of the deployed crew, so killing or stunning them counts as usual, and the driver sits behind the hull, so stopping a vehicle means boarding it or breaking in. Dedicated ranks let modders say who drives, and reuse existing units such as navigators.

**Turned down.**

- A spawner item on the seat in the UFO's map block: it adds units outside the deployment and its difficulty scaling (still possible in a mod).
- Spawn nodes on the seats: map editing for every UFO, and the dice may still leave the seat empty.
- A driver unit spawned by the engine: [GD-02](#gd-02--a-crew-member-drives-no-spawned-driver) turned down spawned drivers.

**Settled with it.** When the driver dies, nobody takes over yet. A crew member walking to the empty seat is a follow-up, judged once this has been played.

### GD-28 · A vehicle's guns need its driver, unless the vehicle says otherwise

Decided · 2026-10-30 · M2, Phase 15

**Problem.** Killing a vehicle's driver stopped it moving, but its mounted guns (Rosigma's turrets, which are units of their own) kept fighting, so the driver mattered little. Some vehicles should still have guns that work by themselves, and a later gunner role may give each gun its own crew.

**Decision.**

- `gunCrew` says who works a vehicle's mounted guns. **`driver`** (the default): the guns work only while someone on board can drive the vehicle; without a driver they have no time units, so they neither act nor react. **`none`**: the guns work by themselves (automated turrets, servitors).
- A gun silenced during a turn stays silent until a new turn starts, even if someone takes the wheel at once.
- The same rule for both sides. For the player, a Chimera's turrets work only while a soldier sits on its driver seat; the craft deployment preview lets you put one there from the start.
- **Later:** `gunners`, each gun worked by a unit on its own gunner seat, and settings per gun, so one vehicle can mix a manned turret and an automated one. The key gets more values then; `driver` stays the default.

**Why.** The driver is the vehicle's crew, so taking them out silences the whole vehicle, which makes boarding or killing the driver the decisive move, as the convoy showcase wants. A default that fits crewed vehicles, with a switch for automated ones, covers mods without per-gun setup for now.

**Turned down.**

- Guns that always keep fighting, the first recommendation: the driver's death should matter more.
- Silencing guns some other way, such as stunning them: they would drop as bodies or items.

**Settled with it.** Silencing works through time units, so a mod's own scripts that hand turrets time units can override it.

**Settled after play (2026-10-30).** At first the guns went silent whenever a vehicle had nobody at the wheel, checked after every step: on the first turn only the Chimera whose seat a soldier happened to start on had a working turret, and moving the driver to another seat silenced the turret. Now a gun goes silent at once only when its driver goes down (killed or stunned), and otherwise at the start of a turn with nobody at the wheel. A battle that starts without a driver, or a driver changing places, costs the guns nothing until then.

### GD-29 · Hijacking: take the seat once the crew is down

Decided · 2026-10-30 · M2, Phase 15

**Decision.**

- A vehicle changes sides when a unit of another side sits on one of its driver seats (anywhere on board if it has none), allowed by the vehicle's `drivers`, and nobody of the vehicle's side on board can still fight (dead, stunned or gone), its mounted guns aside.
- **Its mounted guns change sides with it**, as if mind controlled. At the end of the mission they count as mind-controlled enemies do.
- The vehicle and its guns have no time units until their new side's next turn, so a vehicle can't be taken and driven off in the same turn.
- The player sees "Vehicle captured!", or "The enemy has taken one of your vehicles!".
- The same rule holds for both sides, though the AI doesn't go looking for seats yet (a follow-up, with the crew taking over an empty seat).
- Nothing changes on the Geoscape: a captured vehicle isn't added to your crafts.
- A mod keeps a vehicle from being hijacked by listing in `drivers` only armors the other side doesn't have.

**Why.** "Clear the crew, take the wheel" is a goal players read at once. Taking the guns along makes a capture worth the fight. Waiting a turn keeps it from being a free escape.

**Turned down.**

- Taking the seat with enemies still on board: they would ride along and could take it straight back.
- No hijacking: boarding the command vehicle is the heart of the convoy showcase.

**Settled after the first play (2026-10-30).**

- The guns stay with the vehicle for good. Converted as mind control does, they went back to their old side when the captor's next turn started, so the player never used them. Now, whenever the vehicle's side starts a turn, its mounted guns that went back come over again, with their time units.
- A gun another side mind-controls right now stays theirs until their control ends, then comes back to the vehicle's side. If the old side mind-controls a captured gun, the gun comes back when the vehicle's side starts its next turn, one turn sooner than a mind-controlled soldier would.
- The message says when the vehicle is ready: "Vehicle captured! Ready next turn."

### GD-30 · Turning is a step, and mirroring is free art

Decided · 2026-10-30 · M3, Phase 16 · details [GD-15](#gd-15--turning-only-to-facings-the-art-can-show)

**Decision.**

- **Opt-in.** A vehicle turns only if its `battleVehicle` block sets `front`, the direction its map's front points to (0 north, 2 east, 4 south, 6 west). Without it the vehicle never turns, exactly as in M1 and M2.
- **A turn is a step.** The vehicle is lifted, its layout turned round its middle, checked and set down, all or nothing. It crushes and runs over like a step, is blocked like one, and draws reaction fire like one.
- **Cost.** `tuPerQuarterTurn` per quarter turn (default twice `tuPerTile`), paid by the driver and the vehicle.
- **Mirroring.** Turning between south and east (or north and west) transposes the map along its diagonal:
    - each part is drawn flipped left to right;
    - its hit shape is transposed;
    - west and north walls swap places.
- **What turns with it.** Units and items on board move with their tiles and turn by the same mirror. Driver seats and route nodes follow too.
- **The tiles remember.** A tile part remembers that it is drawn mirrored: one bit per tile part, not saved. Vehicles set it again when a battle is loaded, from their own saved facing.
- **Controls (Phase 16).** In drive mode, a right click turns the vehicle toward the clicked tile, as a right click turns a unit; if a route preview is showing, the right click removes it first, as before. Toward a facing its art can't show, the vehicle lines up with that direction, facing the other way.

**Why.** Treating a turn as a step reuses everything a step already does: fitting, crushing, running over, reaction fire, time units, all or nothing. A bit per tile part costs nothing per frame (the sprite is picked when the part changes) and keeps drawing and hit tests in the engine's own code. Not saving it keeps the save format as it is.

**Turned down.**

- The vehicle keeping its own list of mirrored parts, with tiles untouched: every drawing and hit test would have to ask the vehicles. It stays the fallback if OXCE prefers it (an open question for the maintainers).
- Turning for free, or as part of the next step: a quarter turn of a long vehicle sweeps a lot of ground, and should cost and be checked like a move.

**Settled with it.** Directional big walls on a turning vehicle aren't turned, since their direction belongs to the shared tile definition. No vehicle tileset we know uses them.

### GD-31 · Drives turn the vehicle toward where it goes

Decided · 2026-10-30 · M3, Phase 17 · the user's call, after playing mirror turning and drawn facings

**Decision.**

- **Vehicles that can turn drive along their axis** (`axisLocked`, on by default for a vehicle with `front`): forward or back, also diagonally within 45 degrees, never sideways. A vehicle without `front` slides as before, having no other way to get anywhere.
- **The route plans the turns:** each costs `tuPerQuarterTurn` and needs room for the turned vehicle, like a turn by hand. The preview shows them, and the see-through vehicle stands in its final facing. Turning by hand (right click) stays, for parking.
- **Backing up costs more,** `reverseCost` percent of a step (150 by default), so a long drive turns round and a short one backs up. A vehicle backing toward a facing its art can't show (no back drawn) pays the normal cost.
- **AI drivers use the same routes.**
- **Reversible:** a separate path in the code, its own commits, and `axisLocked: false` per vehicle.

**Why.** With drawn facings, sliding sideways looks wrong, and turning becomes part of driving, with its cost and the room it needs. Long vehicles get less nimble in tight places, which makes positioning matter; play will tell if it feels clunky.

### GD-32 · Up to four drawn facings, mirroring fills the gaps

Decided · 2026-10-30 · M3, Phase 18 · the user's call

**Decision.**

- **`facingMaps`** gives a vehicle a map for each other facing it is drawn in, up to all four. Four drawn maps are the most a vehicle can show, and the showcase Chimera gets all four.
- **Each map has the same layout** turned to its facing, so units, items and damage carry over tile by tile.
- **A missing facing is mirrored from its partner:** east from south and south from east, west from north and north from west.
- **No back:** a vehicle with neither north nor west art can't face away from the camera. Asked to, it lines up with that direction and drives in reverse.

**Why.** Mirroring swaps the two faces seen from the same side, so it can stand in for a missing drawing on that side, but it can never show a back that was never drawn (GD-15). Taking every drawn map a mod has, and mirroring only to fill gaps, gives the best look the art allows. The usual craft, drawn facing the camera only, still turns between its two facings.

**Settled when building it (2026-10-30).**

- **Mirror turning is the default.** Drawn facings only apply to a vehicle whose rule sets `facingMaps`; everything else turns by mirroring as in Phase 16. Phase 18 is a separate path in the code and its own commit, so it can be dropped without touching mirror turning (the user's call: strong for the demo, maybe dropped later).
- **Each facing map lists its own tilesets.** A map file stores each tile part in one byte, and the Chimera craft's own tilesets already use 220 of the 256 numbers, so a facing map can't share them. Each entry of `facingMaps` names its map and its `mapDataSets`; the engine loads those tilesets for the battle.
- **A facing map is the whole craft or UFO map turned,** so a map with several vehicles (three Chimeras) needs one facing map, not one per vehicle.
- **Destroyed parts stay destroyed:** a part gone in the facing a vehicle has when it turns is gone in every facing.
- **The inside is painted too.** With a unit inside, the camera shows the vehicle's level without its roof. The first try reused the old sprites of the parts the painting didn't show, mirrored: in play the walls faced the wrong way, were missing or showed over the hull. Now each drawn facing has two paintings: the full view for the roof, and the view with the roof taken off for the ground level (hull sides and the inside). Parts neither painting shows stay empty, except the floor inside, which keeps its own art turned round (exact for a half turn).
