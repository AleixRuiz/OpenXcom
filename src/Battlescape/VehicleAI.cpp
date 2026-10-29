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
 * Decides where an AI driver takes its vehicle this turn. It holds if anyone on board sees a
 * target the AI would attack. Otherwise it advances toward the nearest enemy its side knows of,
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

}
