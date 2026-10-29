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
#include "VehicleDriveBState.h"
#include "BattlescapeState.h"
#include "Camera.h"
#include "Map.h"
#include "TileEngine.h"
#include "../Engine/Options.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleVehicle.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Sets up a VehicleDriveBState.
 * @param parent Pointer to the Battlescape.
 * @param action Action with the driver as actor (ignoreSpottedEnemies: don't stop for newly spotted enemies).
 * @param vehicle The vehicle to drive.
 * @param path Vehicle positions after each step.
 */
VehicleDriveBState::VehicleDriveBState(BattlescapeGame *parent, BattleAction action, BattleVehicle *vehicle, const std::vector<Position> &path) :
	BattleState(parent, action), _vehicle(vehicle), _path(path), _step(0), _spotted(0), _cancelled(false)
{
}

/**
 * Deletes the VehicleDriveBState.
 */
VehicleDriveBState::~VehicleDriveBState()
{
}

/**
 * Sets the step speed and remembers what the crew has spotted so far.
 */
void VehicleDriveBState::init()
{
	// a bit slower than a walking unit, it moves a whole vehicle each step
	_parent->setStateInterval(Options::battleXcomSpeed * 6);
	_spotted = countSpotted();
}

/**
 * Stops the vehicle after the current step.
 */
void VehicleDriveBState::cancel()
{
	_cancelled = true;
}

/**
 * Counts the enemies spotted this turn by the units on board of the driver's side,
 * the same list walking units use to stop when a new enemy comes into view.
 * @return Number of spotted enemies.
 */
size_t VehicleDriveBState::countSpotted() const
{
	size_t count = 0;
	for (const auto *unit : _vehicle->getPassengers(_parent->getSave()))
	{
		if (unit->getFaction() == _action.actor->getFaction())
		{
			count += unit->getUnitsSpottedThisTurn().size();
		}
	}
	return count;
}

/**
 * Drives one step. Like a walking unit, the vehicle stops when the driver or the vehicle runs out
 * of time units, when something blocks the way, or when the crew spots a new enemy.
 */
void VehicleDriveBState::think()
{
	SavedBattleGame *save = _parent->getSave();
	BattleUnit *driver = _action.actor;
	if (_cancelled || _step >= _path.size() || !driver || !_vehicle->canDrive(save, driver))
	{
		_parent->popState();
		return;
	}

	const Position delta = _path[_step] - _vehicle->getPosition();
	const int cost = _vehicle->getStepCost(save->getMod(), delta);
	if (_vehicle->getTimeUnits() < cost)
	{
		// another driver wouldn't help either
		_action.result = "STR_VEHICLE_OUT_OF_TIME_UNITS";
		_parent->popState();
		return;
	}
	if (driver->getTimeUnits() < cost)
	{
		_action.result = "STR_NOT_ENOUGH_TIME_UNITS";
		_parent->popState();
		return;
	}

	const int level = _vehicle->getPosition().z;
	const BattleVehicle::MoveResult result = _vehicle->move(save, delta);
	if (result != BattleVehicle::MOVE_OK)
	{
		// something got in the way since the route was planned
		_action.result = BattleVehicle::getMoveResultString(result);
		_parent->popState();
		return;
	}
	driver->spendTimeUnits(cost);
	_vehicle->spendTimeUnits(cost);
	++_step;

	TileEngine *tileEngine = save->getTileEngine();
	tileEngine->calculateLighting(LL_AMBIENT, TileEngine::invalid, 0, true);
	tileEngine->recalculateFOV();

	Camera *camera = _parent->getMap()->getCamera();
	if (driver->getFaction() == FACTION_PLAYER && !camera->isOnScreen(driver->getPosition(), true, 0, false))
	{
		camera->centerOnPosition(driver->getPosition());
	}
	if (driver->getFaction() == FACTION_PLAYER && _vehicle->getPosition().z != level)
	{
		// the camera goes up or down a level with the vehicle, as it does with a walking unit
		camera->setViewLevel(driver->getPosition().z);
	}
	save->getBattleState()->updateSoldierInfo(false);

	const size_t spotted = countSpotted();
	if (!_action.ignoreSpottedEnemies && _parent->getPanicHandled() && spotted > _spotted)
	{
		_parent->popState();
		return;
	}
	_spotted = spotted;
}

}
