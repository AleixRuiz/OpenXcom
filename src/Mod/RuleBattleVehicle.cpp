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
#include <algorithm>
#include <sstream>
#include "RuleBattleVehicle.h"
#include "Armor.h"
#include "MapBlock.h"
#include "Mod.h"
#include "RuleDamageType.h"
#include "RuleTerrain.h"
#include "../Engine/Logger.h"

namespace OpenXcom
{

/**
 * Creates a blank battle vehicle ruleset: the craft map splits into its connected pieces,
 * anyone on board can drive, nothing is crushed, units block it, and the vehicle's time
 * units are set from its cost per tile once the rule is loaded.
 */
RuleBattleVehicle::RuleBattleVehicle() : _split(SPLIT_CONNECTED), _tuPerTile(4), _timeUnits(-1), _crushArmor(0),
	_runOverDamage(0), _runOverDamageType(DT_MELEE), _runOverDamageRule(nullptr)
{
}

/**
 * Loads the battle vehicle from YAML. Keys left out keep their current values,
 * so a mod can change part of another mod's block.
 * @param node YAML node.
 */
void RuleBattleVehicle::load(const YAML::YamlNodeReader &node)
{
	node.tryRead("split", _splitName);
	node.tryRead("pieces", _pieces);
	node.tryRead("driverSeats", _driverSeats);
	node.tryRead("drivers", _driverNames);
	node.tryRead("tuPerTile", _tuPerTile);
	node.tryRead("timeUnits", _timeUnits);
	node.tryRead("crushArmor", _crushArmor);
	node.tryRead("runOverDamage", _runOverDamage);
	node.tryRead("runOverDamageType", _runOverDamageType);
}

/**
 * Checks the rule against its craft's battle map blocks and links the driver armors
 * and the damage type of running over.
 * The height of a map block is only known once its map is loaded, so seat levels are
 * checked when the battle starts instead.
 * @param mod Mod for the rule.
 * @param craftType Type of the craft, for error messages.
 * @param terrain The craft's battle terrain.
 */
void RuleBattleVehicle::afterLoad(const Mod *mod, const std::string &craftType, RuleTerrain *terrain)
{
	if (_splitName.empty() || _splitName == "connected")
	{
		_split = SPLIT_CONNECTED;
	}
	else if (_splitName == "rects")
	{
		_split = SPLIT_RECTS;
	}
	else if (_splitName == "whole")
	{
		_split = SPLIT_WHOLE;
	}
	else if (mod->checkForSoftError(true, craftType, "battleVehicle: unknown split '" + _splitName + "', expected connected, rects or whole.", LOG_ERROR))
	{
		_split = SPLIT_CONNECTED;
	}

	if (mod->checkForSoftError(_tuPerTile < 0, craftType, "battleVehicle: tuPerTile cannot be negative.", LOG_ERROR))
	{
		_tuPerTile = 0;
	}
	if (mod->checkForSoftError(_crushArmor < 0, craftType, "battleVehicle: crushArmor cannot be negative.", LOG_ERROR))
	{
		_crushArmor = 0;
	}
	if (mod->checkForSoftError(_runOverDamage < 0, craftType, "battleVehicle: runOverDamage cannot be negative.", LOG_ERROR))
	{
		_runOverDamage = 0;
	}
	if (mod->checkForSoftError(_runOverDamageType < DT_NONE || _runOverDamageType >= DAMAGE_TYPES, craftType, "battleVehicle: unknown runOverDamageType.", LOG_ERROR))
	{
		_runOverDamageType = DT_MELEE;
	}
	_runOverDamageRule = mod->getDamageType((ItemDamageType)_runOverDamageType);
	if (_timeUnits < 0)
	{
		// no limit given: a few straight tiles' worth, so no vehicle drives without a limit by accident
		_timeUnits = DEFAULT_TILES_PER_TURN * _tuPerTile;
	}

	std::vector<MapBlock*> noBlocks;
	const std::vector<MapBlock*> &blocks = terrain ? *terrain->getMapBlocks() : noBlocks;
	mod->checkForSoftError(blocks.empty(), craftType, "battleVehicle: the craft has no battle map blocks.", LOG_ERROR);

	// every piece and seat must fit in every map block the craft may use
	auto fits = [&](int x, int y, int width, int length)
	{
		return std::all_of(blocks.begin(), blocks.end(), [&](const MapBlock *block)
		{
			return x >= 0 && y >= 0 && width > 0 && length > 0 && x + width <= block->getSizeX() && y + length <= block->getSizeY();
		});
	};
	auto describe = [](const std::vector<int> &values)
	{
		std::ostringstream ss;
		ss << "[";
		for (size_t i = 0; i < values.size(); ++i)
		{
			ss << (i ? ", " : "") << values[i];
		}
		ss << "]";
		return ss.str();
	};
	for (auto piece = _pieces.begin(); piece != _pieces.end();)
	{
		const bool ok = piece->size() == 4 && fits(piece->at(0), piece->at(1), piece->at(2), piece->at(3));
		if (!ok && mod->checkForSoftError(true, craftType, "battleVehicle: piece " + describe(*piece) + " is not [x, y, width, length] inside the craft map.", LOG_ERROR))
		{
			piece = _pieces.erase(piece);
			continue;
		}
		++piece;
	}
	for (auto seat = _driverSeats.begin(); seat != _driverSeats.end();)
	{
		const bool ok = seat->size() == 3 && seat->at(2) >= 0 && fits(seat->at(0), seat->at(1), 1, 1);
		if (!ok && mod->checkForSoftError(true, craftType, "battleVehicle: driver seat " + describe(*seat) + " is not [x, y, z] inside the craft map.", LOG_ERROR))
		{
			seat = _driverSeats.erase(seat);
			continue;
		}
		++seat;
	}
	if (_split == SPLIT_RECTS && mod->checkForSoftError(_pieces.empty(), craftType, "battleVehicle: split rects needs at least one piece.", LOG_ERROR))
	{
		_split = SPLIT_CONNECTED;
	}

	mod->linkRule(_drivers, _driverNames);
}

/**
 * Checks whether a unit with this armor can drive the vehicle. With no drivers listed,
 * any unit that can walk drives; units that can't walk (mounted guns) only drive when
 * their armor is listed.
 * @param armor The unit's armor.
 * @return True if it can drive.
 */
bool RuleBattleVehicle::canDrive(const Armor *armor) const
{
	if (!armor)
	{
		return false;
	}
	if (_drivers.empty())
	{
		return armor->allowsMoving();
	}
	return std::find(_drivers.begin(), _drivers.end(), armor) != _drivers.end();
}

}
