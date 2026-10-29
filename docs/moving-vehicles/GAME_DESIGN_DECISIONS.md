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
| [GD-11](#gd-11--ground-vehicles-stay-on-their-level) | Ground vehicles stay on their level | Decided, GD-18 would replace it | M1 |
| [GD-12](#gd-12--no-vehicle-health) | No vehicle health | Decided | M1 |
| [GD-13](#gd-13--craft-maps-first-ufo-vehicles-with-ai-drivers) | Craft maps first, UFO vehicles with AI drivers | Decided | M1, M2 |
| [GD-14](#gd-14--ai-drivers-follow-the-players-rules) | AI drivers follow the player's rules | Decided | M2 |
| [GD-15](#gd-15--turning-only-to-facings-the-art-can-show) | Turning only to facings the art can show | Decided | M3 |
| [GD-16](#gd-16--vehicles-have-their-own-time-units-per-turn) | Vehicles have their own time units per turn | Decided | M1 |
| [GD-17](#gd-17--skilled-and-dedicated-drivers-make-a-vehicle-go-further) | Skilled and dedicated drivers make a vehicle go further | Direction agreed | M2 and later |
| [GD-18](#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them) | Ground vehicles rest on the highest ground under them | Proposed | M1, Phase 7 |

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

Decided · 2026-10-28

**Decision.** A ground vehicle never changes level; slopes and stairs are out of scope. Only flying vehicles (M3) climb or descend.

**Why.** Craft maps are flat, and a vehicle that tilts has no art.

**Revisit.** Hilly maps change level all the time; [GD-18](#gd-18--ground-vehicles-rest-on-the-highest-ground-under-them) proposes how ground vehicles follow them.

### GD-12 · No vehicle health

Decided · 2026-10-28

**Decision.** A vehicle is exactly as broken as its tiles. With no eligible driver on board, it can't move.

**Why.** The hull is terrain, which already takes damage. A second health bar would disagree with what the player sees on the map.

**Revisit when.** We add `criticalParts` (an engine block whose destruction disables the vehicle).

### GD-13 · Craft maps first, UFO vehicles with AI drivers

Decided · 2026-10-28

**Decision.** The MVP drives craft maps only. UFO vehicles come in M2 together with AI drivers, using the same `battleVehicle` block.

**Why.** An enemy vehicle is pointless until the AI can drive it, and a smaller first pitch is easier for OXCE to accept.

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

Proposed · 2026-10-29 · would replace [GD-11](#gd-11--ground-vehicles-stay-on-their-level)

**Problem.** OXCE builds a hill from raised ground: terrain parts with a negative terrain level, in steps of a few voxels (a level is 24), rising over two or three tiles until the hilltop, which is the floor of the level above. Units take it one tile at a time: they step up at most 8 voxels and move up a level once the ground under them is raised 16 or more. A vehicle can't do that. It is a flat, rigid block several tiles long that always sits on a whole level, because its art can't tilt, so on a slope it covers ground of different heights, and on a terraced hill ground of two or three levels.

Today a ground vehicle never changes level, and raised ground isn't in its way, since units can walk on it. So it drives into slopes and they stay inside it: they show through its floor and lift the passengers standing on them. Hilltops are solid terrain below the next level's floor, and stop it or get crushed. This isn't rare: the vanilla terrain files have about 190 raised parts units walk on, in 13 sets (MOUNT, FOREST, DESERT, MARS, POLAR and URBAN among them), and Rosigma's about 3,400, most of them low bumps of 2 to 8 voxels.

**Proposal.** The vehicle rests on the highest ground under it, like a box on an uneven floor.

- **Its level.** For each column of its footprint, find the ground under the vehicle's lowest part in that column, and put the vehicle on the level of the highest. Ground raised less than a full level doesn't lift it; the next level's floor does. Measuring under the lowest part means a Skyranger stands on its landing gear, not on its belly.
- **Raised ground under it is hidden, not crushed.** Slopes and bumps on the vehicle's own level are covered by it while it stands there and come back when it leaves, as floors do today. Walls and objects in the way follow the crush rules ([GD-08](#gd-08--light-terrain-is-crushed-special-terrain-always-blocks)) as before.
- **Lower ground is bridged.** Where the ground falls away, the vehicle hangs over it, as a landed Skyranger's body does. Units can walk underneath; if the vehicle would come down onto them, they block it ([GD-07](#gd-07--units-always-block-in-the-mvp)).
- **Two levels at most.** Its footprint may cover ground of two neighbouring levels, so it climbs or drops one level per step. A drop of two levels or more under any part of it blocks it, so a long vehicle can't cross terraces narrower than it is long.
- **When it climbs.** Uphill, the vehicle goes up a level when its front reaches the hilltop; downhill, it comes down when its rear leaves it.
- **Route.** The route search stays a search over tiles, since the level at each step follows from the ground there. The preview and the ghost show the level the vehicle will be on.

**Why.** Everything higher than the vehicle's own level is a real obstacle, so it never has to hide another level's floor or the units standing on it. One rule covers bumps, slopes, ramps and terraces, the vehicle stays level as its art requires, and players can predict it: a vehicle gets onto a hill when its front does.

**Turned down.**

- Keep GD-11 and make raised ground block: vehicles would be stuck on most outdoor maps, Rosigma's above all.
- Crush raised ground flat: it changes the map for good, and many slopes are too strong anyway (much of Rosigma's raised ground has armor 100 to 255).
- Sit on the ground under its middle, as units do: on a ramp of two or three tiles its front would be buried in the hilltop, hiding the next level's floor and any units on it.
- Tilting vehicles, or a map per slope: art nobody has.
- Vehicles in sections that follow the ground separately: a vehicle is one map with an interior, and splitting it breaks the interior.

**Open.**

- Whether climbing a level costs more than a normal step.
- Whether a long vehicle needs ground under its middle, so it can't balance on its bumper over a drop.
- Whether it may drive off a one-level ledge, or only down slopes.
