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
#include <cstdlib>
#include <functional>
#include <queue>
#include <unordered_map>
#include "BattleVehicle.h"
#include "BattleItem.h"
#include "BattleUnit.h"
#include "Node.h"
#include "SavedBattleGame.h"
#include "Tile.h"
#include "../Battlescape/Pathfinding.h"
#include "../Battlescape/TileEngine.h"
#include "../Engine/Logger.h"
#include "../Mod/Armor.h"
#include "../Mod/MapDataSet.h"
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
		vehicles[k]->buildLookup();
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
	buildLookup();
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
 * Rebuilds the lookup of cells by their offset, after the cells change.
 */
void BattleVehicle::buildLookup()
{
	_size = Position(0, 0, 0);
	for (const auto &cell : _cells)
	{
		_size = Position(std::max(_size.x, (Sint16)(cell.offset.x + 1)), std::max(_size.y, (Sint16)(cell.offset.y + 1)), std::max(_size.z, (Sint16)(cell.offset.z + 1)));
	}
	_cellAt.assign((size_t)_size.x * _size.y * _size.z, -1);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		const Position &o = _cells[i].offset;
		_cellAt[(o.z * _size.y + o.y) * _size.x + o.x] = (int)i;
	}
}

/**
 * Gets the cell of the vehicle at a map position.
 * @param pos Map position.
 * @return Index of the cell, -1 if the vehicle has no tile there.
 */
int BattleVehicle::getCellIndex(Position pos) const
{
	const Position o = pos - _pos;
	if (o.x < 0 || o.y < 0 || o.z < 0 || o.x >= _size.x || o.y >= _size.y || o.z >= _size.z)
	{
		return -1;
	}
	return _cellAt[(o.z * _size.y + o.y) * _size.x + o.x];
}

/**
 * Gets the map data reference of a tile part.
 * @param tile The tile.
 * @param part Part of the tile.
 * @return Map data and tileset indexes.
 */
BattleVehicle::PartRef BattleVehicle::getPart(const Tile *tile, TilePart part)
{
	PartRef ref;
	tile->getMapData(&ref.id, &ref.setId, part);
	return ref;
}

/**
 * Sets the map data of a tile part.
 * @param save Battle save holding the tilesets.
 * @param tile The tile.
 * @param part Part of the tile.
 * @param ref Map data and tileset indexes, -1 for nothing.
 */
void BattleVehicle::setPart(SavedBattleGame *save, Tile *tile, TilePart part, PartRef ref)
{
	if (ref.id >= 0 && ref.setId >= 0 && (size_t)ref.setId < save->getMapDataSets()->size())
	{
		tile->setMapData(save->getMapDataSets()->at(ref.setId)->getObject(ref.id), ref.id, ref.setId, part);
	}
	else
	{
		tile->setMapData(nullptr, -1, -1, part);
	}
}

/**
 * Gets the map data of a part reference.
 * @param save Battle save holding the tilesets.
 * @param ref Map data and tileset indexes.
 * @return The map data, null for nothing.
 */
const MapData *BattleVehicle::getMapData(SavedBattleGame *save, PartRef ref)
{
	if (ref.id >= 0 && ref.setId >= 0 && (size_t)ref.setId < save->getMapDataSets()->size())
	{
		return save->getMapDataSets()->at(ref.setId)->getObject(ref.id);
	}
	return nullptr;
}

/**
 * Checks whether terrain blocks the vehicle: walls, big objects and anything that can't be walked through.
 * @param md Map data of a tile part, or null.
 * @return True if it is in the way.
 */
bool BattleVehicle::isSolid(const MapData *md)
{
	return md && (md->getTUCost(MT_WALK) >= Pathfinding::INVALID_MOVE_COST || md->getBigWall() != 0);
}

/**
 * Checks whether the vehicle can destroy a terrain part: weak enough, and not indestructible,
 * explosive, a lift, or a special part that matters (start and exit tiles, the mission
 * objective, power sources).
 * @param md Map data of a tile part.
 * @param save Battle save, for the mission objective.
 * @param crushArmor Strongest armor the vehicle crushes.
 * @return True if the part can be destroyed.
 */
bool BattleVehicle::canDestroy(const MapData *md, SavedBattleGame *save, int crushArmor)
{
	const SpecialTileType special = md->getSpecialType();
	const bool keep = special == START_POINT || special == END_POINT || special == MUST_DESTROY || special == UFO_POWER_SOURCE
		|| (special != TILE && special == save->getObjectiveType());
	return !keep && md->getArmor() <= crushArmor && md->getArmor() < 255 && md->getExplosive() == 0 && !md->isGravLift();
}

/**
 * Checks whether the vehicle can crush its way through a terrain part: the part and everything
 * it turns into when destroyed must be destroyable, until nothing solid is left.
 * @param md Map data of a tile part.
 * @param save Battle save, for the mission objective.
 * @param crushArmor Strongest armor the vehicle crushes.
 * @return True if the part is out of the way after crushing it.
 */
bool BattleVehicle::canCrush(const MapData *md, SavedBattleGame *save, int crushArmor)
{
	for (int i = 0; i < 8 && isSolid(md); ++i)
	{
		if (!canDestroy(md, save, crushArmor))
		{
			return false;
		}
		md = md->getDieMCD() ? md->getDataset()->getObject(md->getDieMCD()) : nullptr;
	}
	return !isSolid(md);
}

/**
 * Gets the strongest terrain armor the vehicle crushes.
 * @param mod The mod.
 * @return Armor value, 0 without a rule.
 */
int BattleVehicle::getCrushArmor(const Mod *mod) const
{
	const RuleBattleVehicle *rule = getRule(mod);
	return rule ? rule->getCrushArmor() : 0;
}

/**
 * Puts the terrain covered by the vehicle back in place, keeping the vehicle's own parts aside.
 * @param save Battle save.
 * @param carried Gets the vehicle's parts, in their current (possibly damaged) state.
 * @param discovered Gets the fog of war state of each cell.
 */
void BattleVehicle::lift(SavedBattleGame *save, std::vector<PartRef> &carried, std::vector<char> &discovered) const
{
	carried.assign(_cells.size() * O_MAX, PartRef());
	discovered.assign(_cells.size(), 0);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		const Cell &cell = _cells[i];
		Tile *tile = save->getTile(_pos + cell.offset);
		if (!tile)
		{
			continue;
		}
		discovered[i] = tile->isDiscovered(O_FLOOR);
		for (int part = O_FLOOR; part < O_MAX; ++part)
		{
			if (cell.owned & (1 << part))
			{
				const PartRef current = getPart(tile, (TilePart)part);
				// a destroyed part stays destroyed
				if (isOwnSet(current.setId))
				{
					carried[i * O_MAX + part] = current;
				}
				setPart(save, tile, (TilePart)part, cell.under[part]);
			}
		}
	}
}

/**
 * Sets the lifted vehicle down at a position, remembering the terrain it covers there.
 * @param save Battle save.
 * @param pos New vehicle position.
 * @param carried The vehicle's parts, from lift().
 * @param discovered Fog of war state of each cell, from lift().
 */
void BattleVehicle::place(SavedBattleGame *save, Position pos, const std::vector<PartRef> &carried, const std::vector<char> &discovered)
{
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		Cell &cell = _cells[i];
		Tile *tile = save->getTile(pos + cell.offset);
		if (!tile)
		{
			continue;
		}
		for (int part = O_FLOOR; part < O_MAX; ++part)
		{
			if (cell.owned & (1 << part))
			{
				cell.under[part] = getPart(tile, (TilePart)part);
				setPart(save, tile, (TilePart)part, carried[i * O_MAX + part]);
			}
		}
		if (discovered[i])
		{
			tile->setDiscovered(true, O_FLOOR);
		}
	}
	_pos = pos;
}

/**
 * Checks whether the lifted vehicle can be set down at a position. Inside the vehicle (where it
 * has a floor) any solid terrain is in the way; at its edges only the parts it would replace.
 * @param save Battle save.
 * @param pos New vehicle position.
 * @param carried The vehicle's parts, from lift().
 * @param passengers Units that move with the vehicle.
 * @param crushArmor Strongest armor the vehicle crushes.
 * @param toCrush Gets the terrain parts to crush to make room.
 * @return MOVE_OK if it fits, otherwise what blocks it.
 */
BattleVehicle::MoveResult BattleVehicle::checkPlace(SavedBattleGame *save, Position pos, const std::vector<PartRef> &carried, const std::vector<BattleUnit*> &passengers, int crushArmor, std::vector<std::pair<Tile*, TilePart> > &toCrush) const
{
	toCrush.clear();
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		const Cell &cell = _cells[i];
		const Position p = pos + cell.offset;
		Tile *tile = save->getTile(p);
		if (!tile)
		{
			return MOVE_BLOCKED_EDGE;
		}
		for (const auto *other : *save->getVehicles())
		{
			if (other != this && other->covers(p))
			{
				return MOVE_BLOCKED_VEHICLE;
			}
		}
		const BattleUnit *unit = tile->getUnit();
		if (unit && std::find(passengers.begin(), passengers.end(), unit) == passengers.end())
		{
			return MOVE_BLOCKED_UNIT;
		}
		const bool inside = carried[i * O_MAX + O_FLOOR].id != -1;
		for (int part = O_WESTWALL; part < O_MAX; ++part)
		{
			if (!inside && !(cell.owned & (1 << part)))
			{
				continue;
			}
			const MapData *md = tile->getMapData((TilePart)part);
			if (!isSolid(md))
			{
				continue;
			}
			if (!canCrush(md, save, crushArmor))
			{
				return MOVE_BLOCKED_TERRAIN;
			}
			toCrush.push_back(std::make_pair(tile, (TilePart)part));
		}
	}
	return MOVE_OK;
}

/**
 * Gets the center of the vehicle on its lowest level.
 * @return Map position.
 */
Position BattleVehicle::getCenter() const
{
	return _pos + Position((_size.x - 1) / 2, (_size.y - 1) / 2, 0);
}

/**
 * Checks whether a unit standing at a position stands on one of the vehicle's floors.
 * @param save Battle save.
 * @param pos Map position.
 * @return True if the unit would ride along.
 */
bool BattleVehicle::isOnBoard(SavedBattleGame *save, Position pos) const
{
	const int i = getCellIndex(pos);
	const Tile *tile = save->getTile(pos);
	return i != -1 && tile && (_cells[i].owned & (1 << O_FLOOR)) && isOwnSet(getPart(tile, O_FLOOR).setId);
}

/**
 * Gets the units carried by the vehicle: units standing with all their tiles on its floors,
 * and units that can't walk (mounted guns) on any of its tiles.
 * @param save Battle save.
 * @return The units on board.
 */
std::vector<BattleUnit*> BattleVehicle::getPassengers(SavedBattleGame *save) const
{
	std::vector<BattleUnit*> passengers;
	for (const auto &cell : _cells)
	{
		const Tile *tile = save->getTile(_pos + cell.offset);
		BattleUnit *unit = tile ? tile->getUnit() : nullptr;
		if (!unit || std::find(passengers.begin(), passengers.end(), unit) != passengers.end())
		{
			continue;
		}
		const bool mounted = !unit->getArmor()->allowsMoving();
		const int size = unit->getArmor()->getSize();
		bool onBoard = true;
		for (int x = 0; x < size && onBoard; ++x)
		{
			for (int y = 0; y < size && onBoard; ++y)
			{
				const Position p = unit->getPosition() + Position(x, y, 0);
				onBoard = isOnBoard(save, p) || (mounted && covers(p));
			}
		}
		if (onBoard)
		{
			passengers.push_back(unit);
		}
	}
	return passengers;
}

/**
 * Checks whether a unit can drive the vehicle right now: a conscious unit of the vehicle's side,
 * allowed by the rule, standing on a driver seat (or anywhere on board if the vehicle has none).
 * @param save Battle save.
 * @param unit The unit.
 * @return True if it can drive.
 */
bool BattleVehicle::canDrive(SavedBattleGame *save, const BattleUnit *unit) const
{
	const RuleBattleVehicle *rule = getRule(save->getMod());
	if (!rule || !unit || unit->isOut() || !unit->getTile() || unit->getFaction() != _faction || !rule->canDrive(unit->getArmor()))
	{
		return false;
	}
	const Position pos = unit->getPosition();
	if (_seats.empty())
	{
		return isOnBoard(save, pos) || (!unit->getArmor()->allowsMoving() && covers(pos));
	}
	return std::any_of(_seats.begin(), _seats.end(), [&](Position seat) { return _pos + seat == pos; });
}

/**
 * Gets the vehicle a unit can drive.
 * @param save Battle save.
 * @param unit The unit.
 * @return The vehicle, or null if the unit can drive none.
 */
BattleVehicle *BattleVehicle::getDrivenBy(SavedBattleGame *save, const BattleUnit *unit)
{
	for (auto *vehicle : *save->getVehicles())
	{
		if (vehicle->canDrive(save, unit))
		{
			return vehicle;
		}
	}
	return nullptr;
}

/**
 * Gets the driver's time unit cost of a step; diagonal steps cost half as much again.
 * @param mod The mod.
 * @param delta The step.
 * @return Time units.
 */
int BattleVehicle::getStepCost(const Mod *mod, Position delta) const
{
	const RuleBattleVehicle *rule = getRule(mod);
	const int tuPerTile = rule ? rule->getTuPerTile() : 0;
	return (delta.x != 0 && delta.y != 0) ? tuPerTile * 3 / 2 : tuPerTile;
}

/**
 * Moves the vehicle by one step, with the units and items on board and its route nodes.
 * Weak terrain in the way is crushed and stays destroyed. If anything blocks the step,
 * nothing changes.
 * @param save Battle save.
 * @param delta The step.
 * @return MOVE_OK, or what blocked the step.
 */
BattleVehicle::MoveResult BattleVehicle::move(SavedBattleGame *save, Position delta)
{
	if (_cells.empty())
	{
		return MOVE_BLOCKED_EDGE;
	}
	if (delta == Position(0, 0, 0))
	{
		return MOVE_OK;
	}
	const Position oldPos = _pos;
	const Position newPos = _pos + delta;
	const std::vector<BattleUnit*> passengers = getPassengers(save);

	std::vector<BattleItem*> cargo;
	for (const auto &cell : _cells)
	{
		const Position p = oldPos + cell.offset;
		if (isOnBoard(save, p))
		{
			for (auto *item : *save->getTile(p)->getInventory())
			{
				cargo.push_back(item);
			}
		}
	}

	std::vector<PartRef> carried;
	std::vector<char> discovered;
	lift(save, carried, discovered);
	std::vector<std::pair<Tile*, TilePart> > toCrush;
	const MoveResult result = checkPlace(save, newPos, carried, passengers, getCrushArmor(save->getMod()), toCrush);
	if (result != MOVE_OK)
	{
		place(save, oldPos, carried, discovered);
		return result;
	}
	for (const auto &tilePart : toCrush)
	{
		for (int i = 0; i < 8 && isSolid(tilePart.first->getMapData(tilePart.second)); ++i)
		{
			tilePart.first->destroy(tilePart.second, TILE);
		}
	}
	place(save, newPos, carried, discovered);

	// free every old tile first, so passengers don't clear each other's new tiles
	for (auto *unit : passengers)
	{
		unit->setTile(nullptr, save);
	}
	for (auto *unit : passengers)
	{
		const Position p = unit->getPosition() + delta;
		unit->setTile(save->getTile(p), save);
		unit->setPosition(p);
		unit->setPosition(p); // it did not walk, so its last position is the new one too
	}

	const RuleInventory *ground = save->getMod()->getInventoryGround();
	for (auto *item : cargo)
	{
		Tile *from = item->getTile();
		const Position p = from->getPosition() + delta;
		from->removeItem(item);
		save->getTile(p)->addItem(item, ground);
		if (item->getUnit() && item->getUnit()->getPosition() == from->getPosition())
		{
			item->getUnit()->setPosition(p);
		}
	}

	auto *nodes = save->getNodes();
	for (int id : _nodeIds)
	{
		if (id >= 0 && (size_t)id < nodes->size())
		{
			Node *node = nodes->at(id);
			node->setPosition(node->getPosition() + delta);
		}
	}

	// what was left behind on the tiles the vehicle left falls, and so does anything on top of crushed terrain
	for (const auto &cell : _cells)
	{
		save->getTileEngine()->applyGravity(save->getTile(oldPos + cell.offset));
	}
	for (const auto &tilePart : toCrush)
	{
		save->getTileEngine()->applyGravity(save->getTile(tilePart.first->getPosition() + Position(0, 0, 1)));
	}
	return MOVE_OK;
}

/**
 * Finds the steps that take the vehicle to a new position on its level, with A* over vehicle
 * positions: straight steps cost 2, diagonal steps 3 (they can't cut corners), and a step
 * that crushes terrain 2 more. Tiles the vehicle covers now are judged by the terrain under it.
 * @param save Battle save.
 * @param target New vehicle position (its level is ignored).
 * @param path Gets the vehicle positions after each step, the last one being the target.
 * @param error Gets a string ID telling why there is no way.
 * @return True if a way was found.
 */
bool BattleVehicle::findPath(SavedBattleGame *save, Position target, std::vector<Position> &path, std::string &error) const
{
	path.clear();
	target.z = _pos.z;
	if (_cells.empty() || target == _pos)
	{
		error = "STR_VEHICLE_ALREADY_THERE";
		return false;
	}
	const int crushArmor = getCrushArmor(save->getMod());
	const std::vector<BattleUnit*> passengers = getPassengers(save);
	std::vector<char> inside(_cells.size(), 0);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		inside[i] = isOnBoard(save, _pos + _cells[i].offset);
	}

	// -1 if the vehicle doesn't fit with its corner at (x, y), otherwise the number of parts it crushes there
	const int width = save->getMapSizeX(), length = save->getMapSizeY();
	std::unordered_map<int, int> fits;
	auto fitsAt = [&](int x, int y)
	{
		if (x < 0 || y < 0 || x >= width || y >= length)
		{
			return -1;
		}
		auto known = fits.find(y * width + x);
		if (known != fits.end())
		{
			return known->second;
		}
		int crushed = 0;
		for (size_t i = 0; i < _cells.size() && crushed >= 0; ++i)
		{
			const Cell &cell = _cells[i];
			const Position p = Position(x, y, _pos.z) + cell.offset;
			const Tile *tile = save->getTile(p);
			const BattleUnit *unit = tile ? tile->getUnit() : nullptr;
			if (!tile || (unit && std::find(passengers.begin(), passengers.end(), unit) == passengers.end()))
			{
				crushed = -1;
				break;
			}
			for (const auto *other : *save->getVehicles())
			{
				if (other != this && other->covers(p))
				{
					crushed = -1;
				}
			}
			const int current = getCellIndex(p);
			for (int part = O_WESTWALL; part < O_MAX && crushed >= 0; ++part)
			{
				if (!inside[i] && !(cell.owned & (1 << part)))
				{
					continue;
				}
				const MapData *md = tile->getMapData((TilePart)part);
				if (current != -1 && (_cells[current].owned & (1 << part)))
				{
					md = getMapData(save, _cells[current].under[part]);
				}
				if (isSolid(md))
				{
					crushed = canCrush(md, save, crushArmor) ? crushed + 1 : -1;
				}
			}
		}
		fits[y * width + x] = crushed;
		return crushed;
	};
	auto canStep = [&](int x, int y, Position v)
	{
		return fitsAt(x + v.x, y + v.y) >= 0 && (v.x == 0 || v.y == 0 || (fitsAt(x + v.x, y) >= 0 && fitsAt(x, y + v.y) >= 0));
	};
	if (fitsAt(target.x, target.y) < 0)
	{
		error = "STR_VEHICLE_DOES_NOT_FIT_THERE";
		return false;
	}

	auto estimate = [&](int x, int y)
	{
		const int dx = std::abs(x - target.x), dy = std::abs(y - target.y);
		return 2 * std::max(dx, dy) + std::min(dx, dy);
	};
	const int start = _pos.y * width + _pos.x;
	const int goal = target.y * width + target.x;
	std::unordered_map<int, int> cost, from;
	using Entry = std::pair<int, int>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry> > open;
	cost[start] = 0;
	open.push(std::make_pair(estimate(_pos.x, _pos.y), start));
	int expanded = 0;
	while (!open.empty() && expanded < 20000)
	{
		const Entry top = open.top();
		open.pop();
		const int current = top.second;
		const int cx = current % width, cy = current / width;
		if (current == goal)
		{
			break;
		}
		if (top.first - estimate(cx, cy) > cost[current])
		{
			continue; // a better way to this position was found after this entry
		}
		++expanded;
		for (int dir = 0; dir < 8; ++dir)
		{
			Position v;
			Pathfinding::directionToVector(dir, &v);
			if (!canStep(cx, cy, v))
			{
				continue;
			}
			const int next = (cy + v.y) * width + (cx + v.x);
			const int newCost = cost[current] + ((v.x != 0 && v.y != 0) ? 3 : 2) + (fitsAt(cx + v.x, cy + v.y) > 0 ? 2 : 0);
			auto known = cost.find(next);
			if (known == cost.end() || newCost < known->second)
			{
				cost[next] = newCost;
				from[next] = current;
				open.push(std::make_pair(newCost + estimate(cx + v.x, cy + v.y), next));
			}
		}
	}
	if (from.find(goal) == from.end())
	{
		error = "STR_VEHICLE_NO_ROUTE";
		return false;
	}
	for (int at = goal; at != start; at = from[at])
	{
		path.push_back(Position(at % width, at / width, _pos.z));
	}
	std::reverse(path.begin(), path.end());
	return true;
}

/**
 * Gets the message telling why a step of the vehicle failed.
 * @param result What blocked the step.
 * @return A string ID, empty for MOVE_OK.
 */
const char *BattleVehicle::getMoveResultString(MoveResult result)
{
	switch (result)
	{
	case MOVE_BLOCKED_EDGE:
		return "STR_VEHICLE_BLOCKED_BY_MAP_EDGE";
	case MOVE_BLOCKED_VEHICLE:
		return "STR_VEHICLE_BLOCKED_BY_VEHICLE";
	case MOVE_BLOCKED_UNIT:
		return "STR_VEHICLE_BLOCKED_BY_UNIT";
	case MOVE_BLOCKED_TERRAIN:
		return "STR_VEHICLE_BLOCKED_BY_TERRAIN";
	default:
		return "";
	}
}

/**
 * Gets the vehicle's own parts that would stand on a tile if the vehicle were at another
 * position, in their current state, to draw a preview of where a drive ends.
 * @param save Battle save.
 * @param ghostPos Vehicle position to preview.
 * @param tilePos The tile.
 * @param parts Gets the parts on that tile, null where the vehicle has none.
 * @return True if the vehicle would have a part on the tile.
 */
bool BattleVehicle::getGhostParts(SavedBattleGame *save, Position ghostPos, Position tilePos, const MapData *parts[O_MAX]) const
{
	const int i = getCellIndex(tilePos - ghostPos + _pos);
	const Tile *current = i != -1 ? save->getTile(_pos + _cells[i].offset) : nullptr;
	bool any = false;
	for (int part = O_FLOOR; part < O_MAX; ++part)
	{
		parts[part] = nullptr;
		if (current && (_cells[i].owned & (1 << part)))
		{
			const PartRef ref = getPart(current, (TilePart)part);
			if (isOwnSet(ref.setId))
			{
				parts[part] = getMapData(save, ref);
				any = any || parts[part] != nullptr;
			}
		}
	}
	return any;
}

}
