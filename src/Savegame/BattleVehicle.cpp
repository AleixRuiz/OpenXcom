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
#include "BattleVehicle.h"
#include "Node.h"
#include "SavedBattleGame.h"
#include "Tile.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleBattleVehicle.h"
#include "../Mod/RuleCraft.h"

namespace OpenXcom
{

namespace
{

/// Values stored per cell in a save: x, y, z, owned parts, then id and tileset of the covered terrain per part.
const size_t CELL_STRIDE = 4 + 2 * O_MAX;

/// The scorched earth floor the battle generator puts on level 0 tiles without a floor.
const BattleVehicle::PartRef SCORCHED_EARTH = { 1, 0 };

}

/**
 * Remembers the terrain of an area before a craft map is placed there.
 * @param save Battle save.
 * @param corner Lowest corner of the area.
 * @param size Size of the area.
 */
void BattleVehicle::Placement::begin(SavedBattleGame *save, Position corner, Position size)
{
	_corner = corner;
	_size = size;
	_before.assign((size_t)size.x * size.y * size.z * O_MAX, PartRef());
	size_t i = 0;
	for (int z = 0; z < size.z; ++z)
	{
		for (int y = 0; y < size.y; ++y)
		{
			for (int x = 0; x < size.x; ++x, i += O_MAX)
			{
				const Tile *tile = save->getTile(corner + Position(x, y, z));
				if (!tile)
				{
					continue;
				}
				for (int part = O_FLOOR; part < O_MAX; ++part)
				{
					tile->getMapData(&_before[i + part].id, &_before[i + part].setId, (TilePart)part);
				}
			}
		}
	}
	_firstNode = save->getNodes()->size();
}

/**
 * Makes the vehicles out of the tile parts the craft map placed in the area, splitting them
 * as the rule says, and gives each vehicle its driver seats and route nodes.
 * @param save Battle save.
 * @param rule The craft's vehicle rule.
 * @param craftType Type of the craft.
 * @param faction Faction the vehicles belong to.
 * @param firstSetId Index of the craft's first tileset in the battle.
 * @param lastSetId Index after the craft's last tileset in the battle.
 * @param origin Where the craft map's first tile was placed (seats and pieces are relative to it).
 * @return The new vehicles, owned by the caller.
 */
std::vector<BattleVehicle*> BattleVehicle::Placement::finish(SavedBattleGame *save, const RuleBattleVehicle &rule, const std::string &craftType, int faction, int firstSetId, int lastSetId, Position origin) const
{
	// index of a position in the area, -1 outside
	auto areaIndex = [&](Position p)
	{
		const Position d = p - _corner;
		if (d.x < 0 || d.y < 0 || d.z < 0 || d.x >= _size.x || d.y >= _size.y || d.z >= _size.z)
		{
			return -1;
		}
		return (d.z * _size.y + d.y) * _size.x + d.x;
	};

	// the tiles where the craft map placed something
	std::vector<Position> positions;
	std::vector<Cell> cells;
	std::vector<int> cellAt((size_t)_size.x * _size.y * _size.z, -1);
	for (int z = 0; z < _size.z; ++z)
	{
		for (int y = 0; y < _size.y; ++y)
		{
			for (int x = 0; x < _size.x; ++x)
			{
				const Position pos = _corner + Position(x, y, z);
				const Tile *tile = save->getTile(pos);
				if (!tile)
				{
					continue;
				}
				int owned = 0;
				for (int part = O_FLOOR; part < O_MAX; ++part)
				{
					int id, setId;
					tile->getMapData(&id, &setId, (TilePart)part);
					if (setId >= firstSetId && setId < lastSetId)
					{
						owned |= 1 << part;
					}
				}
				if (!owned)
				{
					continue;
				}
				// placing any part of a craft map also removes the terrain object there (see BattlescapeGenerator::loadMAP)
				owned |= 1 << O_OBJECT;

				const size_t before = (size_t)areaIndex(pos) * O_MAX;
				Cell cell;
				cell.offset = pos;
				cell.owned = owned;
				for (int part = O_FLOOR; part < O_MAX; ++part)
				{
					if (owned & (1 << part))
					{
						cell.under[part] = _before[before + part];
					}
				}
				if (pos.z == 0 && cell.under[O_FLOOR].id == -1)
				{
					cell.under[O_FLOOR] = SCORCHED_EARTH;
				}
				cellAt[areaIndex(pos)] = (int)cells.size();
				positions.push_back(pos);
				cells.push_back(cell);
			}
		}
	}
	if (cells.empty())
	{
		Log(LOG_WARNING) << "battleVehicle of " << craftType << ": the craft map placed no tiles, no vehicles made.";
		return {};
	}

	// split the tiles into vehicles
	std::vector<int> pieceOf(cells.size(), -1);
	int pieceCount = 0;
	if (rule.getSplit() == RuleBattleVehicle::SPLIT_WHOLE)
	{
		std::fill(pieceOf.begin(), pieceOf.end(), 0);
		pieceCount = 1;
	}
	else if (rule.getSplit() == RuleBattleVehicle::SPLIT_RECTS)
	{
		// tiles outside every rectangle make one more piece
		const auto &rects = rule.getPieces();
		std::vector<int> pieceOfRect(rects.size() + 1, -1);
		for (size_t i = 0; i < cells.size(); ++i)
		{
			const Position p = positions[i] - origin;
			size_t r = 0;
			while (r < rects.size() && !(p.x >= rects[r][0] && p.y >= rects[r][1] && p.x < rects[r][0] + rects[r][2] && p.y < rects[r][1] + rects[r][3]))
			{
				++r;
			}
			if (pieceOfRect[r] == -1)
			{
				pieceOfRect[r] = pieceCount++;
			}
			pieceOf[i] = pieceOfRect[r];
		}
	}
	else
	{
		// tiles touching each other (also diagonally or on another level) stay together
		for (size_t start = 0; start < cells.size(); ++start)
		{
			if (pieceOf[start] != -1)
			{
				continue;
			}
			std::vector<size_t> open = { start };
			pieceOf[start] = pieceCount;
			while (!open.empty())
			{
				const Position p = positions[open.back()];
				open.pop_back();
				for (int dz = -1; dz <= 1; ++dz)
				{
					for (int dy = -1; dy <= 1; ++dy)
					{
						for (int dx = -1; dx <= 1; ++dx)
						{
							const int index = areaIndex(p + Position(dx, dy, dz));
							if (index >= 0 && cellAt[index] >= 0 && pieceOf[cellAt[index]] == -1)
							{
								pieceOf[cellAt[index]] = pieceCount;
								open.push_back(cellAt[index]);
							}
						}
					}
				}
			}
			++pieceCount;
		}
	}

	// each vehicle starts at its own lowest corner
	std::vector<BattleVehicle*> vehicles;
	std::vector<Position> low(pieceCount, Position(SHRT_MAX, SHRT_MAX, SHRT_MAX));
	for (int k = 0; k < pieceCount; ++k)
	{
		vehicles.push_back(new BattleVehicle(craftType, faction, firstSetId, lastSetId));
	}
	for (size_t i = 0; i < cells.size(); ++i)
	{
		Position &l = low[pieceOf[i]];
		l = Position(std::min(l.x, positions[i].x), std::min(l.y, positions[i].y), std::min(l.z, positions[i].z));
	}
	for (size_t i = 0; i < cells.size(); ++i)
	{
		Cell cell = cells[i];
		cell.offset = positions[i] - low[pieceOf[i]];
		vehicles[pieceOf[i]]->_cells.push_back(cell);
	}
	for (int k = 0; k < pieceCount; ++k)
	{
		vehicles[k]->_pos = low[k];
	}

	// driver seats go to the vehicle they are on
	for (const auto &seat : rule.getDriverSeats())
	{
		const Position p = origin + Position(seat[0], seat[1], seat[2]);
		const int index = areaIndex(p);
		if (index >= 0 && cellAt[index] >= 0)
		{
			const int piece = pieceOf[cellAt[index]];
			vehicles[piece]->_seats.push_back(p - low[piece]);
		}
		else
		{
			Log(LOG_WARNING) << "battleVehicle of " << craftType << ": driver seat [" << seat[0] << ", " << seat[1] << ", " << seat[2] << "] is not on any of its vehicles, ignored.";
		}
	}

	// route nodes added by the craft's routes go with the vehicle they stand on
	auto *nodes = save->getNodes();
	for (size_t n = _firstNode; n < nodes->size(); ++n)
	{
		const Node *node = nodes->at(n);
		const int index = node->isDummy() ? -1 : areaIndex(node->getPosition());
		if (index >= 0 && cellAt[index] >= 0)
		{
			vehicles[pieceOf[cellAt[index]]]->_nodeIds.push_back(node->getID());
		}
	}
	return vehicles;
}

/**
 * Creates an empty vehicle, to be loaded from a save.
 */
BattleVehicle::BattleVehicle() : _faction(0), _firstSetId(-1), _lastSetId(-1)
{
}

/**
 * Creates a vehicle of a craft.
 * @param craftType Type of the craft.
 * @param faction Faction the vehicle belongs to.
 * @param firstSetId Index of the craft's first tileset in the battle.
 * @param lastSetId Index after the craft's last tileset in the battle.
 */
BattleVehicle::BattleVehicle(const std::string &craftType, int faction, int firstSetId, int lastSetId) :
	_craftType(craftType), _faction(faction), _firstSetId(firstSetId), _lastSetId(lastSetId)
{
}

/**
 * Cleans up the vehicle.
 */
BattleVehicle::~BattleVehicle()
{
}

/**
 * Loads the vehicle from YAML.
 * @param reader YAML node.
 */
void BattleVehicle::load(const YAML::YamlNodeReader &reader)
{
	reader.tryRead("craft", _craftType);
	reader.tryRead("faction", _faction);
	reader.tryRead("position", _pos);
	reader.tryRead("firstSetId", _firstSetId);
	reader.tryRead("lastSetId", _lastSetId);
	reader.tryRead("nodes", _nodeIds);

	std::vector<int> seats;
	reader.tryRead("seats", seats);
	_seats.clear();
	for (size_t i = 0; i + 3 <= seats.size(); i += 3)
	{
		_seats.push_back(Position(seats[i], seats[i + 1], seats[i + 2]));
	}

	std::vector<int> data;
	reader.tryRead("cells", data);
	_cells.clear();
	for (size_t i = 0; i + CELL_STRIDE <= data.size(); i += CELL_STRIDE)
	{
		Cell cell;
		cell.offset = Position(data[i], data[i + 1], data[i + 2]);
		cell.owned = data[i + 3];
		for (int part = O_FLOOR; part < O_MAX; ++part)
		{
			cell.under[part].id = data[i + 4 + part * 2];
			cell.under[part].setId = data[i + 5 + part * 2];
		}
		_cells.push_back(cell);
	}
}

/**
 * Saves the vehicle to YAML.
 * @param writer YAML node.
 */
void BattleVehicle::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("craft", _craftType);
	writer.write("faction", _faction);
	writer.write("position", _pos);
	writer.write("firstSetId", _firstSetId);
	writer.write("lastSetId", _lastSetId);
	writer.write("nodes", _nodeIds).setFlowStyle();

	std::vector<int> seats;
	for (const auto &seat : _seats)
	{
		seats.insert(seats.end(), { seat.x, seat.y, seat.z });
	}
	writer.write("seats", seats).setFlowStyle();

	std::vector<int> data;
	data.reserve(_cells.size() * CELL_STRIDE);
	for (const auto &cell : _cells)
	{
		data.insert(data.end(), { cell.offset.x, cell.offset.y, cell.offset.z, cell.owned });
		for (int part = O_FLOOR; part < O_MAX; ++part)
		{
			data.insert(data.end(), { cell.under[part].id, cell.under[part].setId });
		}
	}
	writer.write("cells", data).setFlowStyle();
}

/**
 * Gets the rule of the vehicle, from its craft.
 * @param mod The mod.
 * @return The rule, or null if the craft (or its battleVehicle block) is gone.
 */
const RuleBattleVehicle *BattleVehicle::getRule(const Mod *mod) const
{
	const RuleCraft *craft = mod->getCraft(_craftType);
	return craft ? craft->getBattleVehicle() : nullptr;
}

/**
 * Checks whether the vehicle covers a tile.
 * @param pos Map position.
 * @return True if a part of the tile belongs to the vehicle.
 */
bool BattleVehicle::covers(Position pos) const
{
	return std::any_of(_cells.begin(), _cells.end(), [&](const Cell &cell) { return _pos + cell.offset == pos; });
}

}
