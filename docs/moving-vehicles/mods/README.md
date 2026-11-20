# Moving vehicles demo mods

Two small mods that turn existing crafts into vehicles that drive during battles. They need an OpenXcom Extended build from the `moving-vehicles` branch; any other build ignores their `battleVehicle` blocks.

| Mod | Master | What drives |
| --- | --- | --- |
| `MovingVehiclesVanilla` | UFO Defense (`xcom1`) | The Skyranger, taxiing on its landing gear; the driver seats are the front row of the troop compartment, right behind the cockpit |
| `MovingVehiclesRosigma` | 40k (with Rosigma) | The three Chimeras (also the Steel Legion ones) and the Baneblade; the traitor convoy's three Chimeras, driven by the AI as a convoy heading east |

**Install:** copy the mod folder into the `mods` folder of your OpenXcom user folder (on Windows, `Documents\OpenXcom\mods`), then enable it in the Mods menu, below Rosigma for the Rosigma one.

**Drive:** in a battle, move a soldier onto a driver seat, press the **Drive** button (top right, next to the special action buttons) or the drive hotkey (**D** by default), then click a tile. The first click shows the route, the time units left after each step and a see-through vehicle where it stops; the second click drives. Shift+click keeps driving when a new enemy comes into view. Press Drive again to walk the soldier out. Each vehicle has its own time units per turn (set in the demo rulesets), so another driver can take over, but the vehicle goes no further that turn.

**Skyranger showcase:** the vanilla mod adds a New Battle mission, last in the mission list: "Moving vehicles: Skyranger showcase". A big farm in daylight, the Skyranger on the west side and a landed small scout on the east side, with eight sectoids out in the fields between them. Pick the Skyranger as your craft. It lands nose south. It crushes the walls of farms and town houses (armor up to 40), turns east by mirroring its map, and backs up toward the north and west, which its art can't face.

**Convoy showcase:** the Rosigma mod adds a New Battle mission, last in the mission list: "Moving vehicles: convoy showcase". The traitor convoy, three Chimeras driven by the AI, starts at the west edge of a farm map and drives east to get away. Your Chimeras land north of its way, in the middle, with a few turns to cut it off. Pick the Chimera as your craft. With the showcase's own tiles in the local mod, `facings.rul` lets the Chimera turn all the way round, drawn from behind too; delete it to go back to mirror turning.

The steps to check everything are in [TESTING.md](../TESTING.md).
