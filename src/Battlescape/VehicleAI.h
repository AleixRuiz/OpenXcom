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
#include <vector>
#include "Position.h"

namespace OpenXcom
{

class BattleUnit;
class BattleVehicle;
class SavedBattleGame;

/**
 * Decides how an AI unit drives its vehicle, by the player's rules and with its side's
 * knowledge: hold while the crew has targets, advance to a distance from the nearest enemy
 * it knows of, or stay parked when it knows of none.
 */
class VehicleAI
{
	/// Decides how far a convoy vehicle drives for its exit this turn.
	static bool thinkConvoy(SavedBattleGame *save, BattleUnit *driver, const BattleVehicle *vehicle, int exit, std::vector<Position> &path);
public:
	/// Decides where an AI driver takes its vehicle this turn.
	static bool think(SavedBattleGame *save, BattleUnit *driver, const BattleVehicle *vehicle, std::vector<Position> &path);
	/// Gets the vehicle to drive first: a convoy's lead vehicle, or the vehicle itself.
	static BattleVehicle *getLead(SavedBattleGame *save, BattleVehicle *vehicle, BattleUnit *&driver);
};

}
