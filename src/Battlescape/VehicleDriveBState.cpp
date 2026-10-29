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
#include "VehicleDriveBState.h"
#include "BattlescapeState.h"
#include "Camera.h"
#include "Map.h"
#include "TileEngine.h"
#include "../Engine/Options.h"
#include "../Mod/RuleBattleVehicle.h"
#include "../Mod/RuleDamageType.h"
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
 * Runs over the units in the way of a step: each takes the vehicle's run over damage on the side
 * facing it, credited to the driver. If they all go down, the drive goes on in a new drive state
 * queued behind their deaths, which takes the step once they are out of the way. If one is still
 * standing, the vehicle has rammed it: the step's time units are spent and the drive stops.
 * @param victims The units in the way.
 * @param delta The step.
 * @param cost Time units of the step.
 */
void VehicleDriveBState::runOver(const std::vector<BattleUnit*> &victims, Position delta, int cost)
{
	SavedBattleGame *save = _parent->getSave();
	const RuleBattleVehicle *rule = _vehicle->getRule(save->getMod());
	const RuleDamageType *type = rule->getRunOverDamageType();
	BattleActionAttack attack;
	attack.type = BA_NONE;
	attack.attacker = _action.actor;
	for (auto *victim : victims)
	{
		// hit from the side the vehicle comes from
		victim->damage(Position(-delta.x, -delta.y, 0) * Position::TileXY / 2, type->getRandomDamage(rule->getRunOverDamage()), type, save, attack);
	}
	_parent->checkForCasualties(type, attack);
	if (std::any_of(victims.begin(), victims.end(), [](const BattleUnit *victim) { return !victim->isOutThresholdExceed(); }))
	{
		_action.actor->spendTimeUnits(cost);
		_vehicle->spendTimeUnits(cost);
		_action.result = "STR_VEHICLE_RAMMED";
		_parent->popState();
		return;
	}
	_parent->statePushBack(new VehicleDriveBState(_parent, _action, _vehicle, std::vector<Position>(_path.begin() + _step, _path.end())));
	_parent->popState();
}

/**
 * Lets enemies react to a step: every unit on board of the driving side counts as having taken
 * a step, the driver first, and enemies who see it may fire as at a walking unit.
 * @return True if anyone on board was fired upon.
 */
bool VehicleDriveBState::checkReactionFire()
{
	SavedBattleGame *save = _parent->getSave();
	std::vector<BattleUnit*> crew = _vehicle->getPassengers(save);
	auto driver = std::find(crew.begin(), crew.end(), _action.actor);
	if (driver != crew.end())
	{
		std::rotate(crew.begin(), driver, driver + 1);
	}
	for (auto *unit : crew)
	{
		if (unit->getFaction() != _action.actor->getFaction() || unit->isOut())
		{
			continue;
		}
		BattleAction step;
		step.type = BA_WALK;
		step.actor = unit;
		step.target = unit->getPosition();
		if (save->getTileEngine()->checkReactionFire(unit, step))
		{
			return true;
		}
	}
	return false;
}

/**
 * Drives one step. Like a walking unit, the vehicle stops when the driver or the vehicle runs out
 * of time units, when something blocks the way, when the crew spots a new enemy, or when someone
 * on board is fired upon. Units in the way are run over first (see runOver).
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
	std::vector<BattleUnit*> victims;
	const BattleVehicle::MoveResult result = _vehicle->move(save, delta, victims);
	if (result == BattleVehicle::MOVE_RUN_OVER)
	{
		runOver(victims, delta, cost);
		return;
	}
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
	if (checkReactionFire())
	{
		// someone on board was fired upon: stop, as a walking unit does
		_parent->popState();
		return;
	}
}

}
