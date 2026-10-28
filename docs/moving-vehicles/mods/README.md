# Moving vehicles demo mods

Two small mods that turn existing crafts into vehicles that drive during battles. They need an OpenXcom Extended build from the `moving-vehicles` branch; any other build ignores their `battleVehicle` blocks.

| Mod | Master | What drives |
| --- | --- | --- |
| `MovingVehiclesVanilla` | UFO Defense (`xcom1`) | The Skyranger; the driver seat is the front tile of the cabin |
| `MovingVehiclesRosigma` | 40k (with Rosigma) | The three Chimeras (also the Steel Legion ones) and the Baneblade |

**Install:** copy the mod folder into the `mods` folder of your OpenXcom user folder (on Windows, `Documents\OpenXcom\mods`), then enable it in the Mods menu, below Rosigma for the Rosigma one.

**Drive:** in a battle, move a soldier onto a driver seat, press the **Drive** button (top right, next to the special action buttons) or the drive hotkey (**D** by default), then click a tile. The first click shows the route, the time units left after each step and a see-through vehicle where it stops; the second click drives. Shift+click keeps driving when a new enemy comes into view. Press Drive again to walk the soldier out. Each vehicle has its own time units per turn (set in the demo rulesets), so another driver can take over, but the vehicle goes no further that turn.

The steps to check everything are in [TESTING.md](../TESTING.md).
