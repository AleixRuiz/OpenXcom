#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "../Battlescape/Position.h"
#include "../Mod/MapData.h"
#include "../Engine/Yaml.h"

namespace OpenXcom
{

class BattleUnit;
class Mod;
class RuleBattleVehicle;
class SavedBattleGame;
class Tile;

/**
 * A piece of a craft or UFO map that drives during the battle, with the units and items on board.
 * The vehicle stays plain terrain inside the tiles: this remembers which tile parts belong
 * to it (by tileset, a craft map has its own copy of its tilesets) and what terrain they
 * cover, so it can be lifted off the map and set down somewhere else.
 */
class BattleVehicle
{
public:
	/// Map data reference of one tile part, as stored in a Tile.
	struct PartRef
	{
		int id = -1;
		int setId = -1;
	};
	/// One tile covered by the vehicle.
	struct Cell
	{
		/// Position relative to the vehicle position.
		Position offset;
		/// Bit mask of the tile parts that belong to the vehicle.
		int owned = 0;
		/// Terrain covered by the vehicle, only meaningful for owned parts.
		PartRef under[O_MAX];
	};
	/// What happened to a step of the vehicle.
	enum MoveResult { MOVE_OK, MOVE_BLOCKED_EDGE, MOVE_BLOCKED_VEHICLE, MOVE_BLOCKED_UNIT, MOVE_BLOCKED_TERRAIN, MOVE_TOO_STEEP, MOVE_RUN_OVER };
	/// What a unit standing in the vehicle's way does to it.
	enum UnitInWay { UNIT_IGNORED, UNIT_BLOCKS, UNIT_RUN_OVER };
	/// Decides what a unit standing in the vehicle's way does to it.
	using UnitJudge = std::function<UnitInWay(const BattleUnit*)>;
	/// A place the vehicle can drive to, found by the route search.
	struct Reach
	{
		/// Vehicle position there.
		Position pos;
		/// Time units it takes to get there.
		int tu;
		/// Steps on the way that crush terrain or run over units.
		int crushing;
	};

	/**
	 * Finds the vehicles a craft map holds: remembers the terrain of the area before
	 * the map is placed, then looks at which tile parts came from the craft's tilesets.
	 */
	class Placement
	{
		Position _corner, _size;
		std::vector<PartRef> _before;
		size_t _firstNode = 0;
	public:
		/// Remembers the terrain of an area, call before the craft map is placed there.
		void begin(SavedBattleGame *save, Position corner, Position size);
		/// Makes the vehicles out of what the craft map placed, call after its map and routes are loaded.
		std::vector<BattleVehicle*> finish(SavedBattleGame *save, const RuleBattleVehicle &rule, const std::string &type, bool ufo, int faction, int firstSetId, int lastSetId, Position origin) const;
	};

private:
	std::string _type;
	bool _ufo;
	int _faction;
	int _timeUnits;
	bool _aiDecided;
	int _tuCarry;
	Position _pos;
	int _firstSetId, _lastSetId;
	std::vector<Cell> _cells;
	std::vector<Position> _seats;
	std::vector<int> _nodeIds;
	// cell index for each offset in the vehicle's bounding box, -1 where it has no tile
	Position _size;
	std::vector<int> _cellAt;
	// x and y of each column of the bounding box the vehicle has tiles in, z of its lowest tile there
	std::vector<Position> _columns;

	/// Is the tileset one of the vehicle's?
	bool isOwnSet(int setId) const { return setId >= _firstSetId && setId < _lastSetId; }
	/// Rebuilds the lookup of cells by offset, after the cells change.
	void buildLookup();
	/// Gets the index of the cell at a map position for the current vehicle position, -1 if none.
	int getCellIndex(Position pos) const;
	/// Is a cell the vehicle's lowest tile in its column?
	bool isLowest(const Cell &cell) const;
	/// Gets the map data reference of a tile part.
	static PartRef getPart(const Tile *tile, TilePart part);
	/// Sets the map data of a tile part.
	static void setPart(SavedBattleGame *save, Tile *tile, TilePart part, PartRef ref);
	/// Gets the map data of a part reference.
	static const MapData *getMapData(SavedBattleGame *save, PartRef ref);
	/// Gets a terrain part as if the vehicle were not on the map.
	const MapData *getTerrain(SavedBattleGame *save, Position pos, TilePart part) const;
	/// Does terrain block the vehicle?
	static bool isSolid(const MapData *md);
	/// Is a terrain object ground the vehicle stands on, rather than something in its way?
	bool isGround(SavedBattleGame *save, Position pos, const MapData *object) const;
	/// Gets how high the ground in a tile is.
	int getGroundRaise(SavedBattleGame *save, Position pos) const;
	/// Gets the height of the ground under a tile of the vehicle.
	int getGroundHeight(SavedBattleGame *save, Position pos) const;
	/// Counts the vehicle's columns standing in the ground of the level above it, at a position.
	int countBuried(SavedBattleGame *save, Position pos) const;
	/// Finds the level the vehicle stands on with its lowest corner at a place.
	int findLevel(SavedBattleGame *save, int x, int y, int fromZ, int fromBuried) const;
	/// Can the vehicle destroy a terrain part?
	static bool canDestroy(const MapData *md, SavedBattleGame *save, int crushArmor);
	/// Can the vehicle crush its way through a terrain part (and whatever it turns into)?
	static bool canCrush(const MapData *md, SavedBattleGame *save, int crushArmor);
	/// Gets the strongest terrain armor the vehicle crushes.
	int getCrushArmor(const Mod *mod) const;
	/// Puts the covered terrain back, keeping the vehicle's own parts aside.
	void lift(SavedBattleGame *save, std::vector<PartRef> &carried, std::vector<char> &discovered) const;
	/// Sets the vehicle down at a position, remembering the terrain under it.
	void place(SavedBattleGame *save, Position pos, const std::vector<PartRef> &carried, const std::vector<char> &discovered);
	/// Does the vehicle's side know where a unit is?
	bool knowsOf(const BattleUnit *unit) const;
	/// Decides what a unit in the way does to the vehicle when it gets there.
	UnitInWay judgeUnit(SavedBattleGame *save, const BattleUnit *unit) const;
	/// Checks whether the vehicle can be set down at a position, and what it must crush and run over there.
	MoveResult checkPlace(SavedBattleGame *save, Position pos, const std::vector<char> &inside, const std::vector<BattleUnit*> &passengers, int crushArmor, const UnitJudge &judge, std::vector<std::pair<Tile*, TilePart> > &toCrush, std::vector<BattleUnit*> &victims) const;
	/// Searches the vehicle's routes, to a target or to every place within some time units.
	bool search(SavedBattleGame *save, const Position *target, int budget, std::vector<Position> &path, std::vector<Reach> *reach, std::string &error) const;

public:
	/// Creates an empty vehicle, to be loaded from a save.
	BattleVehicle();
	/// Creates a vehicle of a craft or UFO, owning the tilesets in [firstSetId, lastSetId).
	BattleVehicle(const std::string &type, bool ufo, int faction, int firstSetId, int lastSetId);
	/// Cleans up the vehicle.
	~BattleVehicle();
	/// Loads the vehicle from YAML.
	void load(const YAML::YamlNodeReader &reader, const Mod *mod);
	/// Saves the vehicle to YAML.
	void save(YAML::YamlNodeWriter writer) const;

	/// Gets the type of the craft or UFO the vehicle belongs to.
	const std::string &getType() const { return _type; }
	/// Does the vehicle belong to a UFO rather than a craft?
	bool isUfo() const { return _ufo; }
	/// Gets the rule of the vehicle (null if its craft or UFO no longer defines one).
	const RuleBattleVehicle *getRule(const Mod *mod) const;
	/// Gets the faction the vehicle belongs to.
	int getFaction() const { return _faction; }
	/// Gets the time units the vehicle has left this turn.
	int getTimeUnits() const { return _timeUnits; }
	/// Spends some of the vehicle's time units.
	void spendTimeUnits(int tu);
	/// Gives the vehicle its time units back, at the start of its side's turn.
	void prepareNewTurn(const Mod *mod);
	/// Has an AI unit decided where the vehicle drives this turn?
	bool isAIDecided() const { return _aiDecided; }
	/// Marks that an AI unit has decided where the vehicle drives this turn.
	void setAIDecided() { _aiDecided = true; }
	/// Gets the position of the vehicle's lowest corner.
	Position getPosition() const { return _pos; }
	/// Gets the center of the vehicle, on its lowest level.
	Position getCenter() const;
	/// Gets the tiles covered by the vehicle.
	const std::vector<Cell> &getCells() const { return _cells; }
	/// Does the vehicle cover a tile?
	bool covers(Position pos) const { return getCellIndex(pos) != -1; }
	/// Does a unit standing at a position stand on one of the vehicle's floors?
	bool isOnBoard(SavedBattleGame *save, Position pos) const;
	/// Gets the units carried by the vehicle.
	std::vector<BattleUnit*> getPassengers(SavedBattleGame *save) const;
	/// Is a unit that can't walk mounted on the vehicle?
	bool isMounted(const BattleUnit *unit) const;
	/// Gets the vehicle a unit is mounted on, if any.
	static BattleVehicle *getMountedOn(SavedBattleGame *save, const BattleUnit *unit);

	/// Can a unit drive the vehicle right now?
	bool canDrive(SavedBattleGame *save, const BattleUnit *unit) const;
	/// Gets the vehicle a unit can drive, if any.
	static BattleVehicle *getDrivenBy(SavedBattleGame *save, const BattleUnit *unit);
	/// Gets the time unit cost of a step, paid by the vehicle and its driver.
	int getStepCost(const Mod *mod, Position delta) const;
	/// Gets the time unit cost of a step for a driver with a skill bonus, carrying fractions over between steps.
	int getStepCost(const Mod *mod, Position delta, int bonus, int &carry) const;
	/// Gets how much a driver's skill makes each step cheaper, in percent (negative: dearer).
	int getDriverBonus(SavedBattleGame *save, const BattleUnit *driver) const;
	/// Gets the hundredths of a time unit carried over from the last step.
	int getTuCarry() const { return _tuCarry; }
	/// Sets the hundredths of a time unit carried over to the next step.
	void setTuCarry(int carry) { _tuCarry = carry; }
	/// Moves the vehicle by one step, with everything on board, onto the ground there.
	MoveResult move(SavedBattleGame *save, Position delta, std::vector<BattleUnit*> &victims);
	/// Finds the steps that take the vehicle to a new place, following the ground.
	bool findPath(SavedBattleGame *save, Position target, std::vector<Position> &path, std::string &error) const;
	/// Finds every place the vehicle can drive to with some time units.
	std::vector<Reach> findReachable(SavedBattleGame *save, int tu) const;
	/// Gets the message (a string ID) telling why a step failed.
	static const char *getMoveResultString(MoveResult result);
	/// Gets the vehicle's parts that would stand on a tile if the vehicle were at another position, to preview a drive.
	bool getGhostParts(SavedBattleGame *save, Position ghostPos, Position tilePos, const MapData *parts[O_MAX]) const;
};

}
