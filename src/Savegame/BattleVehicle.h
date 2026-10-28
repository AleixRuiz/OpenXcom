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
#include <string>
#include <vector>
#include "../Battlescape/Position.h"
#include "../Mod/MapData.h"
#include "../Engine/Yaml.h"

namespace OpenXcom
{

class Mod;
class RuleBattleVehicle;
class SavedBattleGame;
class Tile;

/**
 * A piece of a craft map that drives during the battle, with the units and items on board.
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
		std::vector<BattleVehicle*> finish(SavedBattleGame *save, const RuleBattleVehicle &rule, const std::string &craftType, int faction, int firstSetId, int lastSetId, Position origin) const;
	};

private:
	std::string _craftType;
	int _faction;
	Position _pos;
	int _firstSetId, _lastSetId;
	std::vector<Cell> _cells;
	std::vector<Position> _seats;
	std::vector<int> _nodeIds;

	/// Is the tileset one of the vehicle's?
	bool isOwnSet(int setId) const { return setId >= _firstSetId && setId < _lastSetId; }

public:
	/// Creates an empty vehicle, to be loaded from a save.
	BattleVehicle();
	/// Creates a vehicle of a craft, owning the tilesets in [firstSetId, lastSetId).
	BattleVehicle(const std::string &craftType, int faction, int firstSetId, int lastSetId);
	/// Cleans up the vehicle.
	~BattleVehicle();
	/// Loads the vehicle from YAML.
	void load(const YAML::YamlNodeReader &reader);
	/// Saves the vehicle to YAML.
	void save(YAML::YamlNodeWriter writer) const;

	/// Gets the type of the craft the vehicle belongs to.
	const std::string &getCraftType() const { return _craftType; }
	/// Gets the rule of the vehicle (null if its craft no longer defines one).
	const RuleBattleVehicle *getRule(const Mod *mod) const;
	/// Gets the faction the vehicle belongs to.
	int getFaction() const { return _faction; }
	/// Gets the position of the vehicle's lowest corner.
	Position getPosition() const { return _pos; }
	/// Gets the tiles covered by the vehicle.
	const std::vector<Cell> &getCells() const { return _cells; }
	/// Does the vehicle cover a tile?
	bool covers(Position pos) const;
};

}
