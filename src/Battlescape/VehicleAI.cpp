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
#include <climits>
#include <cmath>
#include <string>
#include <tuple>
#include "VehicleAI.h"
#include "AIModule.h"
#include "../Mod/RuleBattleVehicle.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleVehicle.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Decides how far a convoy vehicle drives for its exit this turn: along the cheapest way to the
 * nearest place at that edge of the map (or as close as it can get), preferring ways that crush
 * less, as far as its time units go. Like any AI driver, it takes at most half the driver's time
 * units, so the driver can still fight.
 * @param save Battle save.
 * @param driver The AI unit that can drive the vehicle.
 * @param vehicle The vehicle.
 * @param exit The edge it drives for, as a direction.
 * @param path Gets the steps to drive.
 * @return True if the vehicle should drive.
 */
bool VehicleAI::thinkConvoy(SavedBattleGame *save, BattleUnit *driver, const BattleVehicle *vehicle, int exit, std::vector<Position> &path)
{
	const int budget = std::min(vehicle->getTimeUnits(), driver->getTimeUnits() / 2);
	if (budget <= 0)
	{
		return false;
	}
	// every place it can get to, however many turns away
	const BattleVehicle::Reach *best = nullptr;
	std::tuple<int, int, int> bestScore;
	const std::vector<BattleVehicle::Reach> reach = vehicle->findReachable(save, INT_MAX / 2);
	for (const auto &place : reach)
	{
		const std::tuple<int, int, int> score(vehicle->getEdgeDistance(save, place.pos, exit), place.tu, place.crushing);
		if (!best || score < bestScore)
		{
			best = &place;
			bestScore = score;
		}
	}
	if (!best || std::get<0>(bestScore) >= vehicle->getEdgeDistance(save, vehicle->getPosition(), exit))
	{
		// boxed in: no way gets it closer
		return false;
	}
	std::string error;
	if (!vehicle->findPath(save, best->pos, path, error))
	{
		return false;
	}
	// as far as this turn's time units go
	Position at = vehicle->getPosition();
	int spent = 0;
	size_t steps = 0;
	for (; steps < path.size(); ++steps)
	{
		const int cost = vehicle->getStepCost(save->getMod(), path[steps] - at);
		if (spent + cost > budget)
		{
			break;
		}
		spent += cost;
		at = path[steps];
	}
	path.resize(steps);
	return !path.empty();
}

/**
 * Decides where an AI driver takes its vehicle this turn. A convoy (a vehicle with an exit)
 * always drives for its exit, whatever its crew sees (see thinkConvoy). Any other vehicle holds
 * if anyone on board sees a target the AI would attack. Otherwise it advances toward the nearest enemy its side knows of,
 * to the place it can reach whose middle is closest to the vehicle's keep distance from that
 * enemy, preferring places that crush less, then that cost less. With no known enemy it stays
 * parked. Driving takes at most half the driver's time units, so the driver can still fight.
 * @param save Battle save.
 * @param driver The AI unit that can drive the vehicle.
 * @param vehicle The vehicle.
 * @param path Gets the steps to drive.
 * @return True if the vehicle should drive.
 */
bool VehicleAI::think(SavedBattleGame *save, BattleUnit *driver, const BattleVehicle *vehicle, std::vector<Position> &path)
{
	path.clear();
	const RuleBattleVehicle *rule = vehicle->getRule(save->getMod());
	AIModule *ai = driver->getAIModule();
	if (!rule || !ai)
	{
		return false;
	}
	if (rule->getAIExit() != -1)
	{
		return thinkConvoy(save, driver, vehicle, rule->getAIExit(), path);
	}

	// hold: the crew has something to fight
	for (auto *unit : vehicle->getPassengers(save))
	{
		if (unit->getFaction() != driver->getFaction())
		{
			continue;
		}
		for (auto *seen : *unit->getVisibleUnits())
		{
			if (ai->validTarget(seen, false, true))
			{
				return false;
			}
		}
	}

	// advance: toward the nearest enemy the AI knows of, as its other units know them
	const Position center = vehicle->getCenter();
	const BattleUnit *enemy = nullptr;
	int nearest = INT_MAX;
	for (auto *unit : *save->getUnits())
	{
		if (unit->isOut() || !unit->getTile() || unit->getTurnsSinceSpottedByFaction(driver->getFaction()) > driver->getIntelligence() || !ai->validTarget(unit, false, false))
		{
			continue;
		}
		const int distance = Position::distanceSq(Position(unit->getPosition().x, unit->getPosition().y, 0), Position(center.x, center.y, 0));
		if (distance < nearest)
		{
			nearest = distance;
			enemy = unit;
		}
	}
	if (!enemy)
	{
		// park
		return false;
	}

	const int budget = std::min(vehicle->getTimeUnits(), driver->getTimeUnits() / 2);
	const Position middle = center - vehicle->getPosition();
	const Position target = Position(enemy->getPosition().x, enemy->getPosition().y, 0);
	const float keep = (float)rule->getAIKeepDistance();
	const BattleVehicle::Reach *best = nullptr;
	std::tuple<float, int, int> bestScore;
	const std::vector<BattleVehicle::Reach> reach = vehicle->findReachable(save, budget);
	for (const auto &place : reach)
	{
		const Position at = place.pos + middle;
		const std::tuple<float, int, int> score(std::fabs(Position::distance(Position(at.x, at.y, 0), target) - keep), place.crushing, place.tu);
		if (!best || score < bestScore)
		{
			best = &place;
			bestScore = score;
		}
	}
	if (!best || (best->pos.x == vehicle->getPosition().x && best->pos.y == vehicle->getPosition().y))
	{
		// already where it wants to be
		return false;
	}
	std::string error;
	return vehicle->findPath(save, best->pos, path, error);
}

/**
 * Gets the vehicle to drive first. A convoy drives its lead vehicle first, the one nearest its
 * exit that hasn't driven this turn and has someone at the wheel, so the others don't run into
 * it; any other vehicle drives by itself.
 * @param save Battle save.
 * @param vehicle The vehicle an AI unit can drive.
 * @param driver The AI unit; gets the driver of the vehicle returned.
 * @return The vehicle to drive now.
 */
BattleVehicle *VehicleAI::getLead(SavedBattleGame *save, BattleVehicle *vehicle, BattleUnit *&driver)
{
	const RuleBattleVehicle *rule = vehicle->getRule(save->getMod());
	if (!rule || rule->getAIExit() == -1)
	{
		return vehicle;
	}
	const int exit = rule->getAIExit();
	BattleVehicle *lead = vehicle;
	int nearest = vehicle->getEdgeDistance(save, vehicle->getPosition(), exit);
	for (auto *other : *save->getVehicles())
	{
		const RuleBattleVehicle *otherRule = other->getRule(save->getMod());
		if (other == vehicle || other->getFaction() != vehicle->getFaction() || other->isAIDecided() || !otherRule || otherRule->getAIExit() != exit)
		{
			continue;
		}
		const int distance = other->getEdgeDistance(save, other->getPosition(), exit);
		if (distance >= nearest)
		{
			continue;
		}
		for (auto *unit : other->getPassengers(save))
		{
			if (!unit->isOutThresholdExceed() && other->canDrive(save, unit))
			{
				lead = other;
				nearest = distance;
				driver = unit;
				break;
			}
		}
	}
	return lead;
}

}
