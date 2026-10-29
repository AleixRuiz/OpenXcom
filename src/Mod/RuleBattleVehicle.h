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
#include "../Engine/Yaml.h"

namespace OpenXcom
{

class Armor;
class Mod;
struct RuleDamageType;
class RuleTerrain;

/**
 * Turns the pieces of a craft or UFO map into vehicles that can drive during a battle,
 * with the units and items on board. Read from the craft's or UFO's battleVehicle block.
 */
class RuleBattleVehicle
{
public:
	/// How a map is split into vehicles.
	enum Split { SPLIT_CONNECTED, SPLIT_RECTS, SPLIT_WHOLE };
private:
	std::string _splitName;
	Split _split;
	std::vector<std::vector<int> > _pieces;
	std::vector<std::vector<int> > _driverSeats;
	std::vector<std::string> _driverNames;
	std::vector<const Armor*> _drivers;
	std::vector<int> _driverRanks;
	int _tuPerTile, _timeUnits, _crushArmor;
	int _runOverDamage, _runOverDamageType, _aiKeepDistance;
	int _driverReactionsZeroPoint, _driverReactionsRange;
	const RuleDamageType *_runOverDamageRule;
public:
	/// Time units a vehicle gets per turn when its rule doesn't say, in straight tiles.
	static constexpr int DEFAULT_TILES_PER_TURN = 10;

	/// Creates a blank battle vehicle ruleset.
	RuleBattleVehicle();
	/// Loads the battle vehicle from YAML.
	void load(const YAML::YamlNodeReader &node);
	/// Checks the rule and links it to other rules.
	void afterLoad(const Mod *mod, const std::string &type, RuleTerrain *terrain);

	/// Gets how the map is split into vehicles.
	Split getSplit() const { return _split; }
	/// Gets the rectangles [x, y, width, length] of the pieces, in map tiles (for SPLIT_RECTS).
	const std::vector<std::vector<int> > &getPieces() const { return _pieces; }
	/// Gets the tiles [x, y, z] a unit must stand on to drive, in map tiles; none means anywhere on board.
	const std::vector<std::vector<int> > &getDriverSeats() const { return _driverSeats; }
	/// Can a unit with this armor drive?
	bool canDrive(const Armor *armor) const;
	/// Gets the alien ranks put on a driver seat first when the battle starts (4 is the navigator).
	const std::vector<int> &getDriverRanks() const { return _driverRanks; }
	/// Gets the time unit cost to move the vehicle by one tile, paid by the vehicle and its driver.
	int getTuPerTile() const { return _tuPerTile; }
	/// Gets the vehicle's own time units per turn.
	int getTimeUnits() const { return _timeUnits; }
	/// Gets the strongest terrain armor the vehicle crushes when driving through.
	int getCrushArmor() const { return _crushArmor; }
	/// Gets the damage dealt to a unit the vehicle is driven onto; 0 means units block it.
	int getRunOverDamage() const { return _runOverDamage; }
	/// Gets the damage type dealt to a unit the vehicle is driven onto.
	const RuleDamageType *getRunOverDamageType() const { return _runOverDamageRule; }
	/// Gets the distance in tiles an AI driver keeps from the nearest enemy.
	int getAIKeepDistance() const { return _aiKeepDistance; }
	/// Gets the driver reactions that drive the vehicle with no skill bonus.
	int getDriverReactionsZeroPoint() const { return _driverReactionsZeroPoint; }
	/// Gets the skill range: a driver changes the step cost by (reactions - zero point) * range / 100 percent.
	int getDriverReactionsRange() const { return _driverReactionsRange; }
};

}
