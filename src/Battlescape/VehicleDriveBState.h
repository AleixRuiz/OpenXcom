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
#include "BattleState.h"
#include "Position.h"

namespace OpenXcom
{

class BattleUnit;
class BattleVehicle;

/**
 * Drives a vehicle along a route, one tile per step, with everything on board.
 * The action's actor is the driver; each step costs the same time units to the driver and to the vehicle.
 */
class VehicleDriveBState : public BattleState
{
private:
	BattleVehicle *_vehicle;
	std::vector<Position> _path;
	size_t _step;
	size_t _spotted;
	bool _cancelled;
	/// Counts the enemies the crew has spotted this turn.
	size_t countSpotted() const;
	/// Runs over the units in the way of a step.
	void runOver(const std::vector<BattleUnit*> &victims, Position delta, int cost);
	/// Lets enemies react to the units on board after a step.
	bool checkReactionFire();
public:
	/// Creates a new VehicleDriveBState.
	VehicleDriveBState(BattlescapeGame *parent, BattleAction action, BattleVehicle *vehicle, const std::vector<Position> &path);
	/// Cleans up the VehicleDriveBState.
	~VehicleDriveBState();
	/// Initializes the state.
	void init() override;
	/// Stops after the current step.
	void cancel() override;
	/// Drives one step.
	void think() override;
};

}
