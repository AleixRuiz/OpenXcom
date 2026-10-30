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
#include <tuple>
#include <unordered_map>
#include "BattleVehicle.h"
#include "BattleItem.h"
#include "BattleUnit.h"
#include "Node.h"
#include "SavedBattleGame.h"
#include "Tile.h"
#include "../Battlescape/Pathfinding.h"
#include "../Battlescape/TileEngine.h"
#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"
#include "../Mod/Armor.h"
#include "../Mod/MapDataSet.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleBattleVehicle.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleUfo.h"
#include "Craft.h"
#include "Soldier.h"

namespace OpenXcom
{

namespace
{

/// Values stored per cell in a save: x, y, z, owned parts, then id and tileset of the covered terrain per part.
const size_t CELL_STRIDE = 4 + 2 * O_MAX;

/// The scorched earth floor the battle generator puts on level 0 tiles without a floor.
const BattleVehicle::PartRef SCORCHED_EARTH = { 1, 0 };

/// Ground height of a column with no ground within reach.
const int NO_GROUND = INT_MIN;

/**
 * Turns a tile of a width x length area by quarter turns, north to east, then mirrors it along the
 * diagonal if asked: how a vehicle's own layout becomes its layout in another facing.
 * @param p Tile in the area.
 * @param width Width of the area.
 * @param length Length of the area.
 * @param quarters Quarter turns.
 * @param mirrored Mirror after turning?
 * @return The tile in the turned area.
 */
Position turnTile(Position p, int width, int length, int quarters, bool mirrored)
{
	for (int i = 0; i < quarters; ++i)
	{
		p = Position(length - 1 - p.y, p.x, p.z);
		std::swap(width, length);
	}
	return mirrored ? Position(p.y, p.x, p.z) : p;
}

/**
 * Undoes turnTile: from a tile of the turned area back to the area as it was.
 */
Position unturnTile(Position p, int width, int length, int quarters, bool mirrored)
{
	if (quarters % 2)
	{
		std::swap(width, length);
	}
	if (mirrored)
	{
		p = Position(p.y, p.x, p.z);
	}
	return turnTile(p, width, length, (4 - quarters) % 4, false);
}

/**
 * Turns a wall, a tile's west or north edge, like turnTile turns tiles. A wall turned onto a tile's
 * far edge lands on the next tile's near edge.
 * @param p Tile of the wall.
 * @param slot O_WESTWALL or O_NORTHWALL.
 * @param width Width of the area.
 * @param length Length of the area.
 * @param quarters Quarter turns.
 * @param mirrored Mirror after turning?
 * @param at Gets the tile the wall lands on.
 * @param newSlot Gets which of its walls it is.
 */
void turnWall(Position p, int slot, int width, int length, int quarters, bool mirrored, Position &at, int &newSlot)
{
	// the wall as an edge between two tile corners
	int ax = p.x, ay = p.y;
	int bx = slot == O_WESTWALL ? p.x : p.x + 1, by = slot == O_WESTWALL ? p.y + 1 : p.y;
	for (int i = 0; i < quarters; ++i)
	{
		const int oldAx = ax, oldBx = bx;
		ax = length - ay;
		ay = oldAx;
		bx = length - by;
		by = oldBx;
		std::swap(width, length);
	}
	if (mirrored)
	{
		std::swap(ax, ay);
		std::swap(bx, by);
	}
	if (ax == bx)
	{
		newSlot = O_WESTWALL;
		at = Position(ax, std::min(ay, by), p.z);
	}
	else
	{
		newSlot = O_NORTHWALL;
		at = Position(std::min(ax, bx), ay, p.z);
	}
}

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
 * Makes the vehicles out of the tile parts the craft or UFO map placed in the area, splitting
 * them as the rule says, and gives each vehicle its driver seats and route nodes.
 * @param save Battle save.
 * @param rule The craft's or UFO's vehicle rule.
 * @param type Type of the craft or UFO.
 * @param ufo Is it a UFO's map?
 * @param faction Faction the vehicles belong to.
 * @param firstSetId Index of the craft's first tileset in the battle.
 * @param lastSetId Index after the craft's last tileset in the battle.
 * @param origin Where the craft map's first tile was placed (seats and pieces are relative to it).
 * @return The new vehicles, owned by the caller.
 */
std::vector<BattleVehicle*> BattleVehicle::Placement::finish(SavedBattleGame *save, const RuleBattleVehicle &rule, const std::string &type, bool ufo, int faction, int firstSetId, int lastSetId, Position origin) const
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
		Log(LOG_WARNING) << "battleVehicle of " << type << ": its map placed no tiles, no vehicles made.";
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
		vehicles.push_back(new BattleVehicle(type, ufo, faction, firstSetId, lastSetId));
		vehicles.back()->_timeUnits = rule.getTimeUnits();
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
			Log(LOG_WARNING) << "battleVehicle of " << type << ": driver seat [" << seat[0] << ", " << seat[1] << ", " << seat[2] << "] is not on any of its vehicles, ignored.";
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
BattleVehicle::BattleVehicle() : _ufo(false), _faction(0), _timeUnits(0), _aiDecided(false), _tuCarry(0), _mirrored(false), _firstSetId(-1), _lastSetId(-1), _facing(-1)
{
}

/**
 * Creates a vehicle of a craft or UFO.
 * @param type Type of the craft or UFO.
 * @param ufo Is it a UFO?
 * @param faction Faction the vehicle belongs to.
 * @param firstSetId Index of the map's first tileset in the battle.
 * @param lastSetId Index after the map's last tileset in the battle.
 */
BattleVehicle::BattleVehicle(const std::string &type, bool ufo, int faction, int firstSetId, int lastSetId) :
	_type(type), _ufo(ufo), _faction(faction), _timeUnits(0), _aiDecided(false), _tuCarry(0), _mirrored(false), _firstSetId(firstSetId), _lastSetId(lastSetId), _facing(-1)
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
 * @param mod The mod, for the time units of a vehicle saved without them.
 */
void BattleVehicle::load(const YAML::YamlNodeReader &reader, const Mod *mod)
{
	_ufo = (bool)reader["ufo"];
	reader.tryRead(_ufo ? "ufo" : "craft", _type);
	reader.tryRead("faction", _faction);
	prepareNewTurn(mod);
	reader.tryRead("timeUnits", _timeUnits);
	reader.tryRead("tuCarry", _tuCarry);
	reader.tryRead("mirrored", _mirrored);
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

	if (const auto &facings = reader["facings"])
	{
		facings.tryRead("facing", _facing);
		facings.tryRead("drawn", _drawnFacings);
		facings.tryRead("size", _baseSize);
		facings.tryRead("sets", _extraSetIds);
		std::vector<int> baseSeats;
		facings.tryRead("seats", baseSeats);
		_baseSeats.clear();
		for (size_t i = 0; i + 3 <= baseSeats.size(); i += 3)
		{
			_baseSeats.push_back(Position(baseSeats[i], baseSeats[i + 1], baseSeats[i + 2]));
		}
		std::vector<int> parts;
		facings.tryRead("parts", parts);
		const size_t stride = 5 + 2 * _drawnFacings.size();
		_facingParts.clear();
		for (size_t i = 0; i + stride <= parts.size(); i += stride)
		{
			FacingPart part;
			part.offset = Position(parts[i], parts[i + 1], parts[i + 2]);
			part.slot = parts[i + 3];
			part.lost = parts[i + 4] != 0;
			for (size_t j = 0; j < _drawnFacings.size(); ++j)
			{
				part.art.push_back({ parts[i + 5 + j * 2], parts[i + 6 + j * 2] });
			}
			_facingParts.push_back(part);
		}
	}
}

/**
 * Saves the vehicle to YAML.
 * @param writer YAML node.
 */
void BattleVehicle::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write(_ufo ? "ufo" : "craft", _type);
	writer.write("faction", _faction);
	writer.write("timeUnits", _timeUnits);
	if (_tuCarry != 0)
	{
		writer.write("tuCarry", _tuCarry);
	}
	if (_mirrored)
	{
		writer.write("mirrored", _mirrored);
	}
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

	if (!_drawnFacings.empty())
	{
		auto facings = writer["facings"];
		facings.setAsMap();
		facings.write("facing", _facing);
		facings.write("drawn", _drawnFacings).setFlowStyle();
		facings.write("size", _baseSize);
		facings.write("sets", _extraSetIds).setFlowStyle();
		std::vector<int> baseSeats;
		for (const auto &seat : _baseSeats)
		{
			baseSeats.insert(baseSeats.end(), { seat.x, seat.y, seat.z });
		}
		facings.write("seats", baseSeats).setFlowStyle();
		std::vector<int> parts;
		for (const auto &part : _facingParts)
		{
			parts.insert(parts.end(), { part.offset.x, part.offset.y, part.offset.z, part.slot, part.lost ? 1 : 0 });
			for (const auto &ref : part.art)
			{
				parts.insert(parts.end(), { ref.id, ref.setId });
			}
		}
		facings.write("parts", parts).setFlowStyle();
	}
}

/**
 * Gets the rule of the vehicle, from its craft or UFO.
 * @param mod The mod.
 * @return The rule, or null if the craft or UFO (or its battleVehicle block) is gone.
 */
const RuleBattleVehicle *BattleVehicle::getRule(const Mod *mod) const
{
	if (_ufo)
	{
		const RuleUfo *ufo = mod->getUfo(_type);
		return ufo ? ufo->getBattleVehicle() : nullptr;
	}
	const RuleCraft *craft = mod->getCraft(_type);
	return craft ? craft->getBattleVehicle() : nullptr;
}

/**
 * Spends some of the vehicle's time units.
 * @param tu Time units to spend.
 */
void BattleVehicle::spendTimeUnits(int tu)
{
	_timeUnits = std::max(0, _timeUnits - tu);
}

/**
 * Gives the vehicle all its time units back, at the start of its side's turn, and lets an AI
 * driver decide for it again.
 * @param mod The mod.
 */
void BattleVehicle::prepareNewTurn(const Mod *mod)
{
	const RuleBattleVehicle *rule = getRule(mod);
	_timeUnits = rule ? rule->getTimeUnits() : 0;
	_tuCarry = 0;
	_aiDecided = false;
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
	_columns.clear();
	for (int y = 0; y < _size.y; ++y)
	{
		for (int x = 0; x < _size.x; ++x)
		{
			for (int z = 0; z < _size.z; ++z)
			{
				if (_cellAt[(z * _size.y + y) * _size.x + x] != -1)
				{
					_columns.push_back(Position(x, y, z));
					break;
				}
			}
		}
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
 * Checks whether a cell is the vehicle's lowest tile in its column.
 * @param cell The cell.
 * @return True if the vehicle has no tile below it.
 */
bool BattleVehicle::isLowest(const Cell &cell) const
{
	const Position &o = cell.offset;
	for (int z = 0; z < o.z; ++z)
	{
		if (_cellAt[(z * _size.y + o.y) * _size.x + o.x] != -1)
		{
			return false;
		}
	}
	return true;
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
 * @param flipped Is the part drawn mirrored (see Tile::setFlipped)?
 */
void BattleVehicle::setPart(SavedBattleGame *save, Tile *tile, TilePart part, PartRef ref, bool flipped)
{
	if (ref.id >= 0 && ref.setId >= 0 && (size_t)ref.setId < save->getMapDataSets()->size())
	{
		tile->setMapData(save->getMapDataSets()->at(ref.setId)->getObject(ref.id), ref.id, ref.setId, part);
	}
	else
	{
		tile->setMapData(nullptr, -1, -1, part);
	}
	tile->setFlipped(part, flipped);
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
 * Gets a terrain part as if the vehicle were not on the map: where the vehicle covers it, the part under the vehicle.
 * @param save Battle save.
 * @param pos Map position.
 * @param part Part of the tile.
 * @return The map data, null for nothing or outside the map.
 */
const MapData *BattleVehicle::getTerrain(SavedBattleGame *save, Position pos, TilePart part) const
{
	const Tile *tile = save->getTile(pos);
	if (!tile)
	{
		return nullptr;
	}
	const int i = getCellIndex(pos);
	if (i != -1 && (_cells[i].owned & (1 << part)))
	{
		return getMapData(save, _cells[i].under[part]);
	}
	return tile->getMapData(part);
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
 * Checks whether a terrain object is ground the vehicle stands on rather than something in its way:
 * raised ground units walk on (slopes, hills), and solid parts filling their tile up to the next level
 * with a floor on top (hill rock). Other solid parts are obstacles, crushed or blocking.
 * @param save Battle save.
 * @param pos Map position of the object.
 * @param object Map data of the object, or null.
 * @return True if it is ground.
 */
bool BattleVehicle::isGround(SavedBattleGame *save, Position pos, const MapData *object) const
{
	if (!object || object->getTerrainLevel() >= 0)
	{
		return false;
	}
	if (!isSolid(object))
	{
		return true;
	}
	const MapData *above = getTerrain(save, pos + Position(0, 0, 1), O_FLOOR);
	return object->getTerrainLevel() <= -Position::TileZ && above && !above->isNoFloor();
}

/**
 * Gets how high the ground in a tile is, as the vehicle sees it: its floor, or raised ground on it.
 * @param save Battle save.
 * @param pos Map position.
 * @return Voxels above the bottom of the tile, Position::TileZ for a tile filled up to the next level, -1 if it has no ground.
 */
int BattleVehicle::getGroundRaise(SavedBattleGame *save, Position pos) const
{
	if (!save->getTile(pos))
	{
		return -1;
	}
	int raise = -1;
	const MapData *floor = getTerrain(save, pos, O_FLOOR);
	if (floor && !floor->isNoFloor())
	{
		raise = std::max(0, -floor->getTerrainLevel());
	}
	const MapData *object = getTerrain(save, pos, O_OBJECT);
	if (isGround(save, pos, object))
	{
		raise = std::max(raise, -object->getTerrainLevel());
	}
	return std::min(raise, (int)Position::TileZ);
}

/**
 * Gets the height of the ground under a tile of the vehicle, looking from one level above it down to
 * one level below it. A tile filled with ground (a hill) gives the ground of the level above it.
 * @param save Battle save.
 * @param pos Map position of the vehicle's lowest tile in a column.
 * @return Height in voxels from the bottom of the map, NO_GROUND if the ground is lower than the level below.
 */
int BattleVehicle::getGroundHeight(SavedBattleGame *save, Position pos) const
{
	const int raise = getGroundRaise(save, pos);
	if (raise >= Position::TileZ)
	{
		// the ground goes on in the tile above, two levels up if that one is filled too
		return (pos.z + 1) * Position::TileZ + std::max(0, getGroundRaise(save, pos + Position(0, 0, 1)));
	}
	if (raise >= 0)
	{
		return pos.z * Position::TileZ + raise;
	}
	const int below = getGroundRaise(save, pos - Position(0, 0, 1));
	if (below >= 0)
	{
		return (pos.z - 1) * Position::TileZ + below;
	}
	if (getGroundRaise(save, pos - Position(0, 0, 2)) >= Position::TileZ)
	{
		return (pos.z - 1) * Position::TileZ;
	}
	return NO_GROUND;
}

/**
 * Counts the vehicle's columns standing in the ground of the level above it, as they do after it came off a ledge.
 * @param save Battle save.
 * @param pos Vehicle position.
 * @return Number of columns.
 */
int BattleVehicle::countBuried(SavedBattleGame *save, Position pos) const
{
	int buried = 0;
	for (const auto &column : _columns)
	{
		const Position p = pos + column;
		if (getGroundHeight(save, p) >= (p.z + 1) * Position::TileZ)
		{
			++buried;
		}
	}
	return buried;
}

/**
 * Finds the level the vehicle stands on with its lowest corner at a place. It rests on the highest
 * ground under it, hiding raised ground on its own level and hanging over lower ground, and climbs or
 * comes down one level per step at most. Its middle must be over ground less than a level below it,
 * so it can't climb where its middle would hang; at a ledge it tips over instead and comes down a
 * level, with one end standing in the ledge, and it can drive away from the ledge but not into it.
 * @param save Battle save.
 * @param x X of the vehicle's lowest corner.
 * @param y Y of the vehicle's lowest corner.
 * @param fromZ The level the vehicle comes from.
 * @param fromBuried How many of its columns stand in the ground of the level above it where it comes from.
 * @return The level, or -1 if the ground there is too steep.
 */
int BattleVehicle::findLevel(SavedBattleGame *save, int x, int y, int fromZ, int fromBuried) const
{
	int highest = INT_MIN, lowest = INT_MAX;
	for (const auto &column : _columns)
	{
		const int ground = getGroundHeight(save, Position(x + column.x, y + column.y, fromZ + column.z));
		if (ground == NO_GROUND)
		{
			return -1;
		}
		// a column whose lowest tile is higher up in the vehicle (a craft's belly) can stand on higher ground
		highest = std::max(highest, ground - column.z * Position::TileZ);
		lowest = std::min(lowest, ground);
	}
	int z = highest / Position::TileZ;
	if (std::abs(z - fromZ) > 1)
	{
		return -1;
	}
	const Position middle = getCenter() - _pos;
	const int middleGround = getGroundHeight(save, Position(x + middle.x, y + middle.y, fromZ));
	if (middleGround <= (z - 1) * Position::TileZ)
	{
		// its middle would hang a level or more over the ground: it tips over and comes down a level
		const bool tips = z == fromZ;
		--z;
		if (middleGround <= (z - 1) * Position::TileZ || (!tips && (z != fromZ || countBuried(save, Position(x, y, z)) > fromBuried)))
		{
			return -1;
		}
	}
	if (z < 0 || lowest < (z - 1) * Position::TileZ)
	{
		return -1;
	}
	return z;
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
				setPart(save, tile, (TilePart)part, carried[i * O_MAX + part], _mirrored);
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
 * Checks whether the vehicle's side knows where a unit is, as for walking: its own units, and
 * for the player every unit it sees. A route goes around these; any other unit stops the
 * vehicle when it gets there. The other sides have no drivers yet, so they know every unit.
 * @param unit The unit.
 * @return True if the vehicle's side knows where the unit is.
 */
bool BattleVehicle::knowsOf(const BattleUnit *unit) const
{
	return _faction != FACTION_PLAYER || unit->getFaction() == _faction || unit->getVisible();
}

/**
 * Decides what a unit in the way does to the vehicle when it gets there: units already going
 * down are ignored, the vehicle's own side always blocks, and other units are run over if the
 * vehicle has run over damage, otherwise they block.
 * @param save Battle save.
 * @param unit The unit in the way.
 * @return What the unit does to the vehicle.
 */
BattleVehicle::UnitInWay BattleVehicle::judgeUnit(SavedBattleGame *save, const BattleUnit *unit) const
{
	if (unit->isOut() || unit->isOutThresholdExceed())
	{
		return UNIT_IGNORED;
	}
	const RuleBattleVehicle *rule = getRule(save->getMod());
	if (unit->getFaction() == _faction || !rule || rule->getRunOverDamage() <= 0)
	{
		return UNIT_BLOCKS;
	}
	return UNIT_RUN_OVER;
}

/**
 * Checks whether the vehicle can be set down at a position, judging the terrain as if the vehicle
 * were not on the map. Inside the vehicle (where it has a floor) any solid terrain is in the way;
 * at its edges only the parts it would replace. Ground is never crushed: raised ground on the
 * vehicle's level is hidden under it, and a hill only where the vehicle came off a ledge.
 * @param save Battle save.
 * @param pos New vehicle position.
 * @param inside For each cell, does the vehicle have its floor there?
 * @param passengers Units that move with the vehicle.
 * @param crushArmor Strongest armor the vehicle crushes.
 * @param judge Decides what each unit in the way does to the vehicle (passengers are never in the way).
 * @param toCrush Gets the terrain parts to crush to make room.
 * @param victims Gets the units to run over to make room.
 * @return MOVE_OK if it fits once those are crushed and run over, otherwise what blocks it.
 */
BattleVehicle::MoveResult BattleVehicle::checkPlace(SavedBattleGame *save, Position pos, const std::vector<char> &inside, const std::vector<BattleUnit*> &passengers, int crushArmor, const UnitJudge &judge, std::vector<std::pair<Tile*, TilePart> > &toCrush, std::vector<BattleUnit*> &victims) const
{
	toCrush.clear();
	victims.clear();
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
		BattleUnit *unit = tile->getUnit();
		if (unit && std::find(passengers.begin(), passengers.end(), unit) == passengers.end())
		{
			const UnitInWay inWay = judge(unit);
			if (inWay == UNIT_BLOCKS)
			{
				return MOVE_BLOCKED_UNIT;
			}
			if (inWay == UNIT_RUN_OVER && std::find(victims.begin(), victims.end(), unit) == victims.end())
			{
				victims.push_back(unit);
			}
		}
		for (int part = O_WESTWALL; part < O_MAX; ++part)
		{
			if (!inside[i] && !(cell.owned & (1 << part)))
			{
				continue;
			}
			const MapData *md = getTerrain(save, p, (TilePart)part);
			if (part == O_OBJECT && md && md->getTerrainLevel() <= -Position::TileZ && isGround(save, p, md))
			{
				// a hill: only the bottom of a vehicle that came off a ledge stands in it
				if (isLowest(cell))
				{
					continue;
				}
				return MOVE_BLOCKED_TERRAIN;
			}
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
 * and units mounted on it.
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
		const int size = unit->getArmor()->getSize();
		bool onBoard = true;
		for (int x = 0; x < size && onBoard; ++x)
		{
			for (int y = 0; y < size && onBoard; ++y)
			{
				onBoard = isOnBoard(save, unit->getPosition() + Position(x, y, 0));
			}
		}
		if (onBoard || isMounted(unit))
		{
			passengers.push_back(unit);
		}
	}
	return passengers;
}

/**
 * Checks whether a unit is mounted on the vehicle: it can't walk (a mounted gun) and all its
 * tiles are the vehicle's, floor or not. A mounted unit rides along and is held by the vehicle,
 * so it never falls out, even where the vehicle hangs over lower ground.
 * @param unit The unit.
 * @return True if the unit is mounted on the vehicle.
 */
bool BattleVehicle::isMounted(const BattleUnit *unit) const
{
	if (!unit->getTile() || unit->getArmor()->allowsMoving())
	{
		return false;
	}
	// the unit's tile, not its position: while it rides along, its tile changes first
	const Position pos = unit->getTile()->getPosition();
	const int size = unit->getArmor()->getSize();
	for (int x = 0; x < size; ++x)
	{
		for (int y = 0; y < size; ++y)
		{
			if (!covers(pos + Position(x, y, 0)))
			{
				return false;
			}
		}
	}
	return true;
}

/**
 * Gets the vehicle a unit is mounted on.
 * @param save Battle save.
 * @param unit The unit.
 * @return The vehicle, or null if the unit isn't mounted on one.
 */
BattleVehicle *BattleVehicle::getMountedOn(SavedBattleGame *save, const BattleUnit *unit)
{
	if (unit->getArmor()->allowsMoving())
	{
		return nullptr;
	}
	for (auto *vehicle : *save->getVehicles())
	{
		if (vehicle->isMounted(unit))
		{
			return vehicle;
		}
	}
	return nullptr;
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
 * Puts one of the vehicle's side on a driver seat, when the battle starts, if nobody on its seats
 * can drive. The driver comes from the units already deployed, so how many there are and what they
 * carry doesn't change: a unit of one of the rule's driver ranks first (navigators, say), otherwise
 * any unit but a commander or a leader; those already on board before the others, then the nearest.
 * It must be allowed to drive and fit on a seat. If nobody can, the vehicle stays where it is.
 * @param save Battle save.
 */
void BattleVehicle::seatDriver(SavedBattleGame *save)
{
	const RuleBattleVehicle *rule = getRule(save->getMod());
	if (!rule || _seats.empty())
	{
		return;
	}
	for (const auto &seat : _seats)
	{
		const Tile *tile = save->getTile(_pos + seat);
		if (tile && tile->getUnit() && canDrive(save, tile->getUnit()))
		{
			return;
		}
	}

	const std::vector<int> &ranks = rule->getDriverRanks();
	const Position firstSeat = _pos + _seats.front();
	BattleUnit *driver = nullptr;
	std::tuple<int, int, int> best;
	for (auto *unit : *save->getUnits())
	{
		if (unit->getFaction() != _faction || unit->isOut() || !unit->getTile() || unit->isBigUnit() || !rule->canDrive(unit->getArmor()) || getDrivenBy(save, unit))
		{
			continue;
		}
		const bool dedicated = std::find(ranks.begin(), ranks.end(), unit->getRankInt()) != ranks.end();
		if (!dedicated && unit->getRankInt() <= 1)
		{
			// commanders and leaders ride, they don't drive
			continue;
		}
		const std::tuple<int, int, int> score(dedicated ? 0 : 1, covers(unit->getPosition()) ? 0 : 1, Position::distanceSq(unit->getPosition(), firstSeat));
		if (!driver || score < best)
		{
			driver = unit;
			best = score;
		}
	}
	if (!driver)
	{
		return;
	}
	for (const auto &seat : _seats)
	{
		if (save->setUnitPosition(driver, _pos + seat))
		{
			return;
		}
	}
}

/**
 * Puts a driver on the seats of every vehicle of a side, once the side's units are deployed
 * (see seatDriver).
 * @param save Battle save.
 * @param faction The side.
 */
void BattleVehicle::seatDrivers(SavedBattleGame *save, int faction)
{
	for (auto *vehicle : *save->getVehicles())
	{
		if (vehicle->getFaction() == faction)
		{
			vehicle->seatDriver(save);
		}
	}
}

/**
 * Checks whether anyone on board can drive the vehicle right now: on a seat (or anywhere on board
 * without seats), of its side, allowed by its rule, and not going down.
 * @param save Battle save.
 * @return True if the vehicle has a driver.
 */
bool BattleVehicle::hasDriver(SavedBattleGame *save) const
{
	for (const auto *unit : getPassengers(save))
	{
		if (!unit->isOutThresholdExceed() && canDrive(save, unit))
		{
			return true;
		}
	}
	return false;
}

/**
 * Takes the time units of the vehicle's mounted guns away while the rule says they need a driver
 * and the vehicle has none, so they neither act nor react until someone takes the wheel and a
 * new turn starts.
 * @param save Battle save.
 */
void BattleVehicle::silenceGuns(SavedBattleGame *save) const
{
	const RuleBattleVehicle *rule = getRule(save->getMod());
	if (!rule || rule->getGunCrew() != RuleBattleVehicle::GUNS_NEED_DRIVER || hasDriver(save))
	{
		return;
	}
	for (auto *unit : getPassengers(save))
	{
		if (isMounted(unit) && unit->getFaction() == _faction)
		{
			unit->setTimeUnits(0);
		}
	}
}

/**
 * Hands the vehicle to another side when one of its units sits on a driver seat (anywhere on board
 * without seats), allowed by the rule to drive, and nobody of the vehicle's side on board can
 * fight any more, its mounted guns aside: those change sides with the vehicle. The vehicle and
 * its guns have no time units until their new side's next turn.
 * @param save Battle save.
 * @return The side the vehicle belonged to, or -1 if it didn't change hands.
 */
int BattleVehicle::checkCapture(SavedBattleGame *save)
{
	const RuleBattleVehicle *rule = getRule(save->getMod());
	if (!rule)
	{
		return -1;
	}
	const std::vector<BattleUnit*> onBoard = getPassengers(save);
	BattleUnit *taker = nullptr;
	for (auto *unit : onBoard)
	{
		if (unit->isOut() || unit->isOutThresholdExceed())
		{
			continue;
		}
		if (unit->getFaction() == _faction)
		{
			if (!isMounted(unit))
			{
				// its side still holds it
				return -1;
			}
			continue;
		}
		const Position offset = unit->getPosition() - _pos;
		const bool seated = _seats.empty() || std::find(_seats.begin(), _seats.end(), offset) != _seats.end();
		if (!taker && seated && rule->canDrive(unit->getArmor()))
		{
			taker = unit;
		}
	}
	if (!taker)
	{
		return -1;
	}
	const int from = _faction;
	_faction = taker->getFaction();
	_timeUnits = 0;
	_tuCarry = 0;
	_aiDecided = true;
	for (auto *unit : onBoard)
	{
		if (unit->getFaction() == from && isMounted(unit) && !unit->isOut())
		{
			unit->convertToFaction(taker->getFaction());
			unit->setTimeUnits(0);
		}
	}
	return from;
}

/**
 * Brings back the vehicle's mounted guns that went back to their own side when its side's turn
 * started, and gives them the time units they missed. A captured gun changes sides as a
 * mind-controlled unit does, and those go back at the start of their controller's turn; a
 * captured vehicle keeps its guns instead. Guns another side controls right now are left alone.
 * @param save Battle save.
 */
void BattleVehicle::keepGuns(SavedBattleGame *save) const
{
	for (auto *unit : getPassengers(save))
	{
		if (isMounted(unit) && !unit->isOut() && unit->getFaction() != _faction && unit->getFaction() == unit->getOriginalFaction())
		{
			unit->convertToFaction((UnitFaction)_faction);
			unit->updateUnitStats(true, false);
			if (_faction != FACTION_PLAYER)
			{
				unit->setVisible(false);
			}
		}
	}
}

/**
 * Hands over every vehicle whose seat was taken by another side (see checkCapture), then takes
 * the time units of mounted guns left without a driver (see silenceGuns). Called after units
 * move or go down.
 * @param save Battle save.
 * @return The vehicles that changed hands, each with the side it belonged to.
 */
std::vector<std::pair<BattleVehicle*, int> > BattleVehicle::updateCrews(SavedBattleGame *save)
{
	std::vector<std::pair<BattleVehicle*, int> > captured;
	for (auto *vehicle : *save->getVehicles())
	{
		const int from = vehicle->checkCapture(save);
		if (from != -1)
		{
			captured.push_back(std::make_pair(vehicle, from));
		}
		vehicle->silenceGuns(save);
	}
	return captured;
}

/**
 * Gets how many tiles the vehicle would be from a map edge, standing at a position: 0 when it
 * touches the edge.
 * @param save Battle save.
 * @param pos Vehicle position.
 * @param edge The edge, as a direction (0 north, 2 east, 4 south, 6 west).
 * @return The distance in tiles.
 */
int BattleVehicle::getEdgeDistance(SavedBattleGame *save, Position pos, int edge) const
{
	int distance = INT_MAX;
	for (const auto &column : _columns)
	{
		const int x = pos.x + column.x, y = pos.y + column.y;
		switch (edge)
		{
		case 0: distance = std::min(distance, y); break;
		case 2: distance = std::min(distance, save->getMapSizeX() - 1 - x); break;
		case 4: distance = std::min(distance, save->getMapSizeY() - 1 - y); break;
		default: distance = std::min(distance, x); break;
		}
	}
	return distance;
}

/**
 * Takes the vehicle off the battle through the map edge, as a convoy escapes. The units of its
 * side on board, mounted guns and stunned ones included, and the items on board leave with it:
 * they go into the time out of units left behind between the stages of a mission, so they count
 * as neither killed nor captured, and the items are lost. Anyone else on board, and their bodies,
 * stay on the ground where they were. The terrain under the vehicle comes back. The caller then
 * deletes the vehicle.
 * @param save Battle save.
 */
void BattleVehicle::leaveBattle(SavedBattleGame *save)
{
	std::vector<BattleUnit*> leaving;
	for (auto *unit : getPassengers(save))
	{
		if (unit->getFaction() == _faction)
		{
			leaving.push_back(unit);
		}
	}
	std::vector<BattleItem*> cargo;
	for (const auto &cell : _cells)
	{
		const Position p = _pos + cell.offset;
		if (!isOnBoard(save, p))
		{
			continue;
		}
		for (auto *item : *save->getTile(p)->getInventory())
		{
			BattleUnit *body = item->getUnit();
			if (body && body->getFaction() != _faction)
			{
				continue;
			}
			cargo.push_back(item);
			if (body && body->getStatus() == STATUS_UNCONSCIOUS)
			{
				leaving.push_back(body);
			}
		}
	}
	for (auto *item : cargo)
	{
		save->removeItem(item);
	}
	for (auto *unit : leaving)
	{
		unit->setTile(nullptr, save);
		unit->setPosition(TileEngine::invalid, false);
		unit->setFire(0);
		unit->goToTimeOut();
	}

	std::vector<PartRef> carried;
	std::vector<char> discovered;
	lift(save, carried, discovered);
	// whoever stays behind on the roof comes down
	for (const auto &cell : _cells)
	{
		save->getTileEngine()->applyGravity(save->getTile(_pos + cell.offset));
	}
}

/**
 * Takes the convoy vehicles of a side that stand at the map edge they drive for off the battle,
 * as long as someone on board can still drive them (see leaveBattle). Only the AI sides' vehicles
 * leave: a convoy vehicle the player captured stays.
 * @param save Battle save.
 * @param faction The side whose vehicles may leave.
 * @return How many vehicles left.
 */
int BattleVehicle::escapeAtExits(SavedBattleGame *save, int faction)
{
	int escaped = 0;
	if (faction == FACTION_PLAYER)
	{
		return escaped;
	}
	auto *vehicles = save->getVehicles();
	for (auto i = vehicles->begin(); i != vehicles->end();)
	{
		BattleVehicle *vehicle = *i;
		const RuleBattleVehicle *rule = vehicle->getRule(save->getMod());
		if (vehicle->_faction == faction && rule && rule->getAIExit() != -1
			&& vehicle->getEdgeDistance(save, vehicle->_pos, rule->getAIExit()) == 0 && vehicle->hasDriver(save))
		{
			vehicle->leaveBattle(save);
			delete vehicle;
			i = vehicles->erase(i);
			++escaped;
		}
		else
		{
			++i;
		}
	}
	return escaped;
}

/**
 * Gets the time unit cost of a step, paid by the vehicle and its driver; a diagonal step and
 * climbing a level each cost half as much again.
 * @param mod The mod.
 * @param delta The step.
 * @return Time units.
 */
int BattleVehicle::getStepCost(const Mod *mod, Position delta) const
{
	const RuleBattleVehicle *rule = getRule(mod);
	int cost = rule ? rule->getTuPerTile() : 0;
	if (delta.x != 0 && delta.y != 0)
	{
		cost = cost * 3 / 2;
	}
	if (delta.z > 0)
	{
		cost = cost * 3 / 2;
	}
	return cost;
}

/**
 * Gets the time unit cost of a step for a driver with a skill bonus: the step's cost made cheaper
 * by the bonus (dearer if it is negative). Fractions of a time unit carry over to the next step,
 * so a small bonus still counts over a drive.
 * @param mod The mod.
 * @param delta The step.
 * @param bonus The driver's bonus, in percent (see getDriverBonus).
 * @param carry Hundredths of a time unit carried over from the last step; gets what carries over to the next.
 * @return Time units.
 */
int BattleVehicle::getStepCost(const Mod *mod, Position delta, int bonus, int &carry) const
{
	const int hundredths = getStepCost(mod, delta) * (100 - bonus) + carry;
	carry = hundredths % 100;
	return hundredths / 100;
}

/**
 * Gets how much a driver's skill makes each step cheaper, as piloting skill does in air combat:
 * (reactions - zero point) * range / 100 percent, capped at 50 either way. Only soldiers have
 * driving skill. If the vehicle's craft takes pilots, only its pilots drive with their skill;
 * anyone else drives as a novice, with no bonus.
 * @param save Battle save.
 * @param driver The driver.
 * @return The bonus in percent, negative if the step gets dearer.
 */
int BattleVehicle::getDriverBonus(SavedBattleGame *save, const BattleUnit *driver) const
{
	const RuleBattleVehicle *rule = getRule(save->getMod());
	Soldier *soldier = driver ? driver->getGeoscapeSoldier() : nullptr;
	if (!rule || !soldier)
	{
		return 0;
	}
	Craft *craft = soldier->getCraft();
	if (!_ufo && craft && craft->getRules()->getType() == _type && craft->getRules()->getPilots() > 0)
	{
		const std::vector<Soldier*> pilots = craft->getPilotList(false, nullptr);
		if (std::find(pilots.begin(), pilots.end(), soldier) == pilots.end())
		{
			return 0;
		}
	}
	const int bonus = (driver->getBaseStats()->reactions - rule->getDriverReactionsZeroPoint()) * rule->getDriverReactionsRange() / 100;
	return std::max(-50, std::min(50, bonus));
}

/**
 * Moves the vehicle by one step, with the units and items on board and its route nodes. It
 * goes up or down to the level of the ground there (see findLevel). Weak terrain in the way
 * is crushed and stays destroyed. If anything blocks the step, nothing changes; units to run
 * over are handed back without moving, for the caller to deal with first.
 * @param save Battle save.
 * @param delta The step; its level is ignored, the ground decides it.
 * @param victims Gets the units in the way to run over, with MOVE_RUN_OVER.
 * @return MOVE_OK, MOVE_RUN_OVER, or what blocked the step.
 */
BattleVehicle::MoveResult BattleVehicle::move(SavedBattleGame *save, Position delta, std::vector<BattleUnit*> &victims)
{
	victims.clear();
	if (_cells.empty())
	{
		return MOVE_BLOCKED_EDGE;
	}
	if (delta.x == 0 && delta.y == 0)
	{
		return MOVE_OK;
	}
	const Position oldPos = _pos;
	const int z = findLevel(save, oldPos.x + delta.x, oldPos.y + delta.y, oldPos.z, countBuried(save, oldPos));
	if (z < 0)
	{
		return MOVE_TOO_STEEP;
	}
	delta.z = z - oldPos.z;
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
	std::vector<char> inside(_cells.size(), 0);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		inside[i] = carried[i * O_MAX + O_FLOOR].id != -1;
	}
	std::vector<std::pair<Tile*, TilePart> > toCrush;
	const MoveResult result = checkPlace(save, newPos, inside, passengers, getCrushArmor(save->getMod()), [&](const BattleUnit *unit) { return judgeUnit(save, unit); }, toCrush, victims);
	if (result != MOVE_OK || !victims.empty())
	{
		place(save, oldPos, carried, discovered);
		return result != MOVE_OK ? result : MOVE_RUN_OVER;
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
 * Mirrors a direction along the map's diagonal: north and west swap, south and east too, and so
 * do north-east and south-west. Units on a vehicle turned by mirroring its map turn this way.
 * @param direction Direction, 0 to 7.
 * @return The mirrored direction.
 */
int BattleVehicle::mirrorDirection(int direction)
{
	return (14 - direction) % 8;
}

/**
 * Gets the direction the vehicle's front points to now: its map's front, mirrored if its layout is.
 * @param mod The mod.
 * @return The facing (0 north, 2 east, 4 south, 6 west), or -1 if the vehicle can't turn.
 */
int BattleVehicle::getFacing(const Mod *mod) const
{
	const RuleBattleVehicle *rule = getRule(mod);
	if (!rule || rule->getFront() == -1)
	{
		return -1;
	}
	if (hasDrawnFacings())
	{
		return _facing;
	}
	return _mirrored ? mirrorDirection(rule->getFront()) : rule->getFront();
}

/**
 * Gets the facing the vehicle takes when turned toward a direction. Mirroring only swaps the two
 * facings drawn from the same side, so the vehicle lines up with the direction: where its front
 * can't point that way, its back does.
 * @param mod The mod.
 * @param direction The direction (0 north, 2 east, 4 south, 6 west).
 * @return The facing it takes, its current one if it is already lined up, or -1 if it can't turn.
 */
int BattleVehicle::getFacingToward(const Mod *mod, int direction) const
{
	const int facing = getFacing(mod);
	if (facing != -1 && hasDrawnFacings())
	{
		// drawn in more facings: that way if it can face it, otherwise back first
		int quarters;
		bool mirrored;
		if (findArt(direction, quarters, mirrored) != -1)
		{
			return direction;
		}
		const int back = (direction + 4) % 8;
		return findArt(back, quarters, mirrored) != -1 ? back : facing;
	}
	if (facing == -1 || facing % 4 == direction % 4)
	{
		return facing;
	}
	return mirrorDirection(facing);
}

/**
 * Gets the time unit cost of turning the vehicle to a facing, paid by the vehicle and its driver:
 * tuPerQuarterTurn for each quarter turn, made cheaper or dearer by the driver's skill as a step is.
 * @param mod The mod.
 * @param facing The facing to turn to.
 * @param bonus The driver's bonus, in percent (see getDriverBonus).
 * @param carry Hundredths of a time unit carried over from the last step; gets what carries over to the next.
 * @return Time units.
 */
int BattleVehicle::getTurnCost(const Mod *mod, int facing, int bonus, int &carry) const
{
	const RuleBattleVehicle *rule = getRule(mod);
	const int current = getFacing(mod);
	if (!rule || current == -1)
	{
		return 0;
	}
	const int eighths = (facing - current + 8) % 8;
	const int quarters = std::min(eighths, 8 - eighths) / 2;
	const int hundredths = quarters * rule->getTuPerQuarterTurn() * (100 - bonus) + carry;
	carry = hundredths % 100;
	return hundredths / 100;
}

/**
 * Remembers what lies on the map under the vehicle's layout, at its position, as the terrain under
 * it. Only right after lifting the vehicle, when the map shows the terrain again and the layout
 * has changed (see turn).
 * @param save Battle save.
 */
void BattleVehicle::readUnder(SavedBattleGame *save)
{
	for (auto &cell : _cells)
	{
		const Tile *tile = save->getTile(_pos + cell.offset);
		for (int part = O_FLOOR; part < O_MAX; ++part)
		{
			if (cell.owned & (1 << part))
			{
				cell.under[part] = tile ? getPart(tile, (TilePart)part) : PartRef();
			}
		}
	}
}

/**
 * Mirrors the vehicle's layout along its diagonal, as for a quarter turn between two facings drawn
 * from the same side: each cell goes to the transposed offset, and its west and north walls swap
 * places, with the parts carried for it. Driver seats follow.
 * @param carried The vehicle's parts, from lift(), swapped along.
 */
void BattleVehicle::mirrorLayout(std::vector<PartRef> &carried)
{
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		Cell &cell = _cells[i];
		cell.offset = Position(cell.offset.y, cell.offset.x, cell.offset.z);
		const bool west = (cell.owned & (1 << O_WESTWALL)) != 0;
		const bool north = (cell.owned & (1 << O_NORTHWALL)) != 0;
		cell.owned &= ~((1 << O_WESTWALL) | (1 << O_NORTHWALL));
		cell.owned |= (west ? 1 << O_NORTHWALL : 0) | (north ? 1 << O_WESTWALL : 0);
		std::swap(cell.under[O_WESTWALL], cell.under[O_NORTHWALL]);
		std::swap(carried[i * O_MAX + O_WESTWALL], carried[i * O_MAX + O_NORTHWALL]);
	}
	for (auto &seat : _seats)
	{
		seat = Position(seat.y, seat.x, seat.z);
	}
	_mirrored = !_mirrored;
	buildLookup();
}

/**
 * Turns the vehicle to a facing round its middle, with the units and items on board and its route
 * nodes, onto the ground there. Turning between the two facings drawn from the same side mirrors
 * its layout (see mirrorLayout): its parts are drawn flipped, and everything on board goes to the
 * mirrored tile and turns by the same mirror. A turn is a step: weak terrain in the way is crushed,
 * and if anything blocks it nothing changes; units to run over are handed back without turning,
 * for the caller to deal with first.
 * @param save Battle save.
 * @param facing The facing to turn to (see getFacingToward).
 * @param victims Gets the units in the way to run over, with MOVE_RUN_OVER.
 * @return MOVE_OK, MOVE_RUN_OVER, or what blocked the turn.
 */
BattleVehicle::MoveResult BattleVehicle::turn(SavedBattleGame *save, int facing, std::vector<BattleUnit*> &victims)
{
	victims.clear();
	if (hasDrawnFacings())
	{
		return turnDrawn(save, facing, victims);
	}
	const int current = getFacing(save->getMod());
	if (_cells.empty() || current == -1)
	{
		return MOVE_BLOCKED_EDGE;
	}
	if (facing == current)
	{
		return MOVE_OK;
	}
	const Position oldPos = _pos;
	const Position middle = getCenter();
	const int buried = countBuried(save, oldPos);
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
	std::vector<char> inside(_cells.size(), 0);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		inside[i] = carried[i * O_MAX + O_FLOOR].id != -1;
	}
	mirrorLayout(carried);
	readUnder(save);

	// round its middle, onto the ground there
	Position newPos = middle - Position((_size.x - 1) / 2, (_size.y - 1) / 2, 0);
	newPos.z = oldPos.z;
	MoveResult result = MOVE_BLOCKED_EDGE;
	std::vector<std::pair<Tile*, TilePart> > toCrush;
	if (newPos.x >= 0 && newPos.y >= 0 && newPos.x + _size.x <= save->getMapSizeX() && newPos.y + _size.y <= save->getMapSizeY())
	{
		newPos.z = findLevel(save, newPos.x, newPos.y, oldPos.z, buried);
		result = newPos.z < 0 ? MOVE_TOO_STEEP : checkPlace(save, newPos, inside, passengers, getCrushArmor(save->getMod()), [&](const BattleUnit *unit) { return judgeUnit(save, unit); }, toCrush, victims);
	}
	if (result != MOVE_OK || !victims.empty())
	{
		mirrorLayout(carried);
		place(save, oldPos, carried, discovered);
		return result != MOVE_OK ? result : MOVE_RUN_OVER;
	}
	for (const auto &tilePart : toCrush)
	{
		for (int i = 0; i < 8 && isSolid(tilePart.first->getMapData(tilePart.second)); ++i)
		{
			tilePart.first->destroy(tilePart.second, TILE);
		}
	}
	place(save, newPos, carried, discovered);

	// everything on board goes to its mirrored tile
	auto mirrored = [&](Position p)
	{
		const Position offset = p - oldPos;
		return newPos + Position(offset.y, offset.x, offset.z);
	};
	// free every old tile first, so passengers don't clear each other's new tiles
	for (auto *unit : passengers)
	{
		unit->setTile(nullptr, save);
	}
	for (auto *unit : passengers)
	{
		const Position p = mirrored(unit->getPosition());
		unit->setTile(save->getTile(p), save);
		unit->setPosition(p);
		unit->setPosition(p); // it did not walk, so its last position is the new one too
		unit->setDirection(mirrorDirection(unit->getDirection()));
	}

	const RuleInventory *ground = save->getMod()->getInventoryGround();
	for (auto *item : cargo)
	{
		Tile *from = item->getTile();
		const Position p = mirrored(from->getPosition());
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
			node->setPosition(mirrored(node->getPosition()));
		}
	}

	// what was left behind on the tiles the vehicle left falls, and so does anything on top of crushed terrain
	for (const auto &cell : _cells)
	{
		save->getTileEngine()->applyGravity(save->getTile(oldPos + Position(cell.offset.y, cell.offset.x, cell.offset.z)));
	}
	for (const auto &tilePart : toCrush)
	{
		save->getTileEngine()->applyGravity(save->getTile(tilePart.first->getPosition() + Position(0, 0, 1)));
	}
	return MOVE_OK;
}

/**
 * Finds the drawn facing that shows a facing: the facing itself if it is drawn, otherwise its mirror
 * partner (south for east, north for west), mirrored.
 * @param facing The facing.
 * @param quarters Gets the quarter turns from the vehicle's map to the drawn facing.
 * @param mirrored Gets whether the drawn facing is mirrored.
 * @return Index of the drawn facing (in FacingPart::art), or -1 if the vehicle can't show that facing.
 */
int BattleVehicle::findArt(int facing, int &quarters, bool &mirrored) const
{
	if (_drawnFacings.empty())
	{
		return -1;
	}
	for (int pass = 0; pass < 2; ++pass)
	{
		const int drawnFacing = pass == 0 ? facing : mirrorDirection(facing);
		auto it = std::find(_drawnFacings.begin(), _drawnFacings.end(), drawnFacing);
		if (it != _drawnFacings.end())
		{
			quarters = ((drawnFacing - _drawnFacings.front() + 8) % 8) / 2;
			mirrored = pass == 1;
			return (int)(it - _drawnFacings.begin());
		}
	}
	return -1;
}

/**
 * Gets where a part of the vehicle's own layout goes in a facing (see turnTile and turnWall).
 * @param part The part.
 * @param quarters Quarter turns from the vehicle's map.
 * @param mirrored Mirrored after turning?
 * @param at Gets its tile, relative to the vehicle's position.
 * @param slot Gets its tile part.
 */
void BattleVehicle::placePart(const FacingPart &part, int quarters, bool mirrored, Position &at, int &slot) const
{
	if (part.slot == O_WESTWALL || part.slot == O_NORTHWALL)
	{
		turnWall(part.offset, part.slot, _baseSize.x, _baseSize.y, quarters, mirrored, at, slot);
	}
	else
	{
		at = turnTile(part.offset, _baseSize.x, _baseSize.y, quarters, mirrored);
		slot = part.slot;
	}
}

/**
 * Turns a vehicle drawn in more facings round its middle, like turn() does for mirroring: its parts go
 * where the new facing puts them, drawn as that facing shows them (see findArt), and everything on
 * board goes to the matching tile and turns with it. Parts destroyed in any facing stay gone. A turn
 * is a step: weak terrain in the way is crushed, and if anything blocks it nothing changes; units to
 * run over are handed back without turning, for the caller to deal with first.
 * @param save Battle save.
 * @param facing The facing to turn to.
 * @param victims Gets the units in the way to run over, with MOVE_RUN_OVER.
 * @return MOVE_OK, MOVE_RUN_OVER, or what blocked the turn.
 */
BattleVehicle::MoveResult BattleVehicle::turnDrawn(SavedBattleGame *save, int facing, std::vector<BattleUnit*> &victims)
{
	int quarters, oldQuarters;
	bool mirrored, oldMirrored;
	const int art = findArt(facing, quarters, mirrored);
	if (_cells.empty() || art < 0 || findArt(_facing, oldQuarters, oldMirrored) < 0)
	{
		return MOVE_BLOCKED_EDGE;
	}
	if (facing == _facing)
	{
		return MOVE_OK;
	}

	// a part destroyed in the facing the vehicle has now stays gone in the others
	for (auto &part : _facingParts)
	{
		Position at;
		int slot;
		placePart(part, oldQuarters, oldMirrored, at, slot);
		const Tile *tile = save->getTile(_pos + at);
		if (!tile || !isOwnSet(getPart(tile, (TilePart)slot).setId))
		{
			part.lost = true;
		}
	}

	// the layout's size in a facing, for turning round its middle
	auto turnedSize = [&](int q, bool m)
	{
		return ((q % 2 == 1) != m) ? Position(_baseSize.y, _baseSize.x, 0) : Position(_baseSize.x, _baseSize.y, 0);
	};
	const Position oldPos = _pos;
	const Position oldSize = turnedSize(oldQuarters, oldMirrored);
	const Position middle = oldPos + Position((oldSize.x - 1) / 2, (oldSize.y - 1) / 2, 0);
	const int buried = countBuried(save, oldPos);
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
	const std::vector<Cell> oldCells = _cells;
	const bool wasDiscovered = std::any_of(discovered.begin(), discovered.end(), [](char d) { return d != 0; });

	// the new layout: every part where the facing puts it, drawn as that facing shows it
	std::vector<Cell> cells;
	std::vector<PartRef> parts;
	for (const auto &part : _facingParts)
	{
		Position at;
		int slot;
		placePart(part, quarters, mirrored, at, slot);
		size_t i = 0;
		while (i < cells.size() && !(cells[i].offset == at))
		{
			++i;
		}
		if (i == cells.size())
		{
			Cell cell;
			cell.offset = at;
			// the vehicle takes the object slot of every tile it covers, as its map did
			cell.owned = 1 << O_OBJECT;
			cells.push_back(cell);
			parts.resize(cells.size() * O_MAX);
		}
		cells[i].owned |= 1 << slot;
		parts[i * O_MAX + slot] = part.lost ? PartRef() : part.art[art];
	}
	_cells = cells;
	_mirrored = mirrored;
	buildLookup();
	readUnder(save);
	std::vector<char> inside(_cells.size(), 0);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		inside[i] = parts[i * O_MAX + O_FLOOR].id != -1;
	}
	const std::vector<char> seen(_cells.size(), wasDiscovered ? 1 : 0);

	// round its middle, onto the ground there
	const Position newSize = turnedSize(quarters, mirrored);
	Position newPos = middle - Position((newSize.x - 1) / 2, (newSize.y - 1) / 2, 0);
	newPos.z = oldPos.z;
	MoveResult result = MOVE_BLOCKED_EDGE;
	std::vector<std::pair<Tile*, TilePart> > toCrush;
	if (newPos.x >= 0 && newPos.y >= 0 && newPos.x + _size.x <= save->getMapSizeX() && newPos.y + _size.y <= save->getMapSizeY())
	{
		newPos.z = findLevel(save, newPos.x, newPos.y, oldPos.z, buried);
		result = newPos.z < 0 ? MOVE_TOO_STEEP : checkPlace(save, newPos, inside, passengers, getCrushArmor(save->getMod()), [&](const BattleUnit *unit) { return judgeUnit(save, unit); }, toCrush, victims);
	}
	if (result != MOVE_OK || !victims.empty())
	{
		_cells = oldCells;
		_mirrored = oldMirrored;
		buildLookup();
		place(save, oldPos, carried, discovered);
		return result != MOVE_OK ? result : MOVE_RUN_OVER;
	}
	for (const auto &tilePart : toCrush)
	{
		for (int i = 0; i < 8 && isSolid(tilePart.first->getMapData(tilePart.second)); ++i)
		{
			tilePart.first->destroy(tilePart.second, TILE);
		}
	}
	place(save, newPos, parts, seen);
	_facing = facing;
	_seats.clear();
	for (const auto &seat : _baseSeats)
	{
		_seats.push_back(turnTile(seat, _baseSize.x, _baseSize.y, quarters, mirrored));
	}

	// everything on board goes to the matching tile, back to the vehicle's own layout and out to the new facing
	auto moveTile = [&](Position p)
	{
		const Position own = unturnTile(p - oldPos, _baseSize.x, _baseSize.y, oldQuarters, oldMirrored);
		return newPos + turnTile(own, _baseSize.x, _baseSize.y, quarters, mirrored);
	};
	auto turnDirection = [&](int direction)
	{
		if (oldMirrored)
		{
			direction = mirrorDirection(direction);
		}
		direction = (direction + 2 * (quarters - oldQuarters) + 8) % 8;
		return mirrored ? mirrorDirection(direction) : direction;
	};
	// free every old tile first, so passengers don't clear each other's new tiles
	for (auto *unit : passengers)
	{
		unit->setTile(nullptr, save);
	}
	for (auto *unit : passengers)
	{
		// a big unit stands on the corner of its tiles nearest to the map's corner
		const int size = unit->getArmor()->getSize();
		Position p = moveTile(unit->getPosition());
		for (int x = 0; x < size; ++x)
		{
			for (int y = 0; y < size; ++y)
			{
				const Position q = moveTile(unit->getPosition() + Position(x, y, 0));
				p = Position(std::min(p.x, q.x), std::min(p.y, q.y), p.z);
			}
		}
		unit->setTile(save->getTile(p), save);
		unit->setPosition(p);
		unit->setPosition(p); // it did not walk, so its last position is the new one too
		unit->setDirection(turnDirection(unit->getDirection()));
	}

	const RuleInventory *ground = save->getMod()->getInventoryGround();
	for (auto *item : cargo)
	{
		Tile *from = item->getTile();
		const Position p = moveTile(from->getPosition());
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
			node->setPosition(moveTile(node->getPosition()));
		}
	}

	// what was left behind on the tiles the vehicle left falls, and so does anything on top of crushed terrain
	for (const auto &cell : oldCells)
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
 * Gives the vehicles a craft or UFO map makes the maps they are drawn with in other facings, if their
 * rule has facingMaps. Each is the whole craft or UFO map turned to its facing, drawn with tilesets of
 * its own, which are loaded for the battle. Each vehicle then remembers, for every part of its own
 * layout, its part in each drawn facing. A facing whose map or tilesets are missing is left out, with
 * an error in the log. Call right after the vehicles are made (see Placement::finish).
 * @param save Battle save.
 * @param mod The mod, to load the tilesets.
 * @param rule The vehicles' rule.
 * @param vehicles The vehicles the map made.
 * @param origin Where the map's first tile was placed.
 * @param mapSize Size of the map.
 */
void BattleVehicle::loadFacingMaps(SavedBattleGame *save, Mod *mod, const RuleBattleVehicle &rule, const std::vector<BattleVehicle*> &vehicles, Position origin, Position mapSize)
{
	if (rule.getFacingMaps().empty() || rule.getFront() == -1 || vehicles.empty())
	{
		return;
	}
	struct Drawn
	{
		int facing, quarters, sizeX, sizeY, sizeZ;
		std::vector<PartRef> parts;
	};
	std::vector<Drawn> drawn;
	std::vector<int> extraSetIds;
	for (const auto &facingMap : rule.getFacingMaps())
	{
		const std::string filename = "MAPS/" + facingMap.map + ".MAP";
		bool found = FileMap::fileExists(filename);
		for (const auto &name : facingMap.mapDataSets)
		{
			found = found && FileMap::fileExists("TERRAIN/" + name + ".MCD");
		}
		if (!found || facingMap.facing < 0 || facingMap.facing > 6 || facingMap.facing % 2 != 0)
		{
			Log(LOG_ERROR) << "battleVehicle of " << vehicles.front()->_type << ": the map for facing " << facingMap.facing << " (" << filename << ") or one of its tilesets is missing; it doesn't show that facing.";
			continue;
		}
		// its tilesets, loaded for the battle
		std::vector<std::pair<int, size_t> > sets;
		for (const auto &name : facingMap.mapDataSets)
		{
			MapDataSet *mds = mod->getMapDataSet(name);
			mds->loadData(mod->getMCDPatch(name));
			save->getMapDataSets()->push_back(mds);
			sets.push_back(std::make_pair((int)save->getMapDataSets()->size() - 1, mds->getSize()));
			extraSetIds.push_back(sets.back().first);
		}
		Drawn d;
		d.facing = facingMap.facing;
		d.quarters = ((facingMap.facing - rule.getFront() + 8) % 8) / 2;
		auto file = FileMap::getIStream(filename);
		unsigned char size[3];
		file->read((char*)size, sizeof(size));
		d.sizeY = size[0];
		d.sizeX = size[1];
		d.sizeZ = size[2];
		if (d.sizeX != (d.quarters % 2 ? mapSize.y : mapSize.x) || d.sizeY != (d.quarters % 2 ? mapSize.x : mapSize.y))
		{
			Log(LOG_ERROR) << "battleVehicle of " << vehicles.front()->_type << ": " << filename << " is not its map turned to facing " << facingMap.facing << "; it doesn't show that facing.";
			continue;
		}
		d.parts.assign((size_t)d.sizeX * d.sizeY * d.sizeZ * O_MAX, PartRef());
		for (int z = d.sizeZ - 1; z >= 0; --z)
		{
			for (int y = 0; y < d.sizeY; ++y)
			{
				for (int x = 0; x < d.sizeX; ++x)
				{
					unsigned char value[O_MAX];
					file->read((char*)value, sizeof(value));
					for (int part = O_FLOOR; part < O_MAX; ++part)
					{
						int index = value[part];
						for (size_t s = 0; s < sets.size() && index > 0; ++s)
						{
							if ((size_t)index < sets[s].second)
							{
								d.parts[(((size_t)z * d.sizeY + y) * d.sizeX + x) * O_MAX + part] = { index, sets[s].first };
								break;
							}
							index -= (int)sets[s].second;
						}
					}
				}
			}
		}
		drawn.push_back(d);
	}
	if (drawn.empty())
	{
		return;
	}

	for (auto *vehicle : vehicles)
	{
		vehicle->_facing = rule.getFront();
		vehicle->_drawnFacings = { rule.getFront() };
		for (const auto &d : drawn)
		{
			vehicle->_drawnFacings.push_back(d.facing);
		}
		vehicle->_baseSize = vehicle->_size;
		vehicle->_baseSeats = vehicle->_seats;
		vehicle->_extraSetIds = extraSetIds;
		vehicle->_facingParts.clear();
		for (const auto &cell : vehicle->_cells)
		{
			const Tile *tile = save->getTile(vehicle->_pos + cell.offset);
			for (int slot = O_FLOOR; slot < O_MAX && tile; ++slot)
			{
				const PartRef base = getPart(tile, (TilePart)slot);
				if (!(cell.owned & (1 << slot)) || !vehicle->isOwnSet(base.setId))
				{
					continue;
				}
				FacingPart part;
				part.offset = cell.offset;
				part.slot = slot;
				part.art.push_back(base);
				// the part's place on its map, turned with the map
				const Position onMap = vehicle->_pos + cell.offset - origin;
				for (const auto &d : drawn)
				{
					Position at;
					int atSlot = slot;
					if (slot == O_WESTWALL || slot == O_NORTHWALL)
					{
						turnWall(onMap, slot, mapSize.x, mapSize.y, d.quarters, false, at, atSlot);
					}
					else
					{
						at = turnTile(onMap, mapSize.x, mapSize.y, d.quarters, false);
					}
					const bool onDrawnMap = at.x >= 0 && at.y >= 0 && at.z >= 0 && at.x < d.sizeX && at.y < d.sizeY && at.z < d.sizeZ;
					part.art.push_back(onDrawnMap ? d.parts[(((size_t)at.z * d.sizeY + at.y) * d.sizeX + at.x) * O_MAX + atSlot] : PartRef());
				}
				vehicle->_facingParts.push_back(part);
			}
		}
	}
}

/**
 * Draws the vehicle's parts mirrored again if its layout is, after a battle is loaded: tiles
 * don't save it.
 * @param save Battle save.
 */
void BattleVehicle::flipParts(SavedBattleGame *save) const
{
	if (!_mirrored)
	{
		return;
	}
	for (const auto &cell : _cells)
	{
		Tile *tile = save->getTile(_pos + cell.offset);
		for (int part = O_FLOOR; part < O_MAX && tile; ++part)
		{
			if (cell.owned & (1 << part))
			{
				tile->setFlipped((TilePart)part, true);
			}
		}
	}
}

/**
 * Searches the vehicle's routes over vehicle positions, the level of each position following from
 * the ground there (see findLevel): A* to a target, or every place within some time units. Straight
 * steps cost 2, diagonal steps 3 (they can't cut corners), climbing a level half as much again, a
 * step that crushes terrain 2 more and one that runs over units 6 more. Tiles the vehicle covers now
 * are judged by the terrain under it. Only the units the vehicle's side knows of are in the way; any
 * other unit stops the drive, or is run over, when the vehicle gets there. The player's vehicles
 * only run over the units under the target place; other sides' run over whatever is in the way.
 * @param save Battle save.
 * @param target New vehicle position (its level is ignored), or null to find every place within the budget.
 * @param budget Time units the vehicle may spend, without a target.
 * @param path Gets the vehicle positions after each step to the target, the last one being the target.
 * @param reach Gets the places within the budget, without a target.
 * @param error Gets a string ID telling why there is no way to the target.
 * @return True if a way to the target was found.
 */
bool BattleVehicle::search(SavedBattleGame *save, const Position *target, int budget, std::vector<Position> &path, std::vector<Reach> *reach, std::string &error) const
{
	path.clear();
	if (_cells.empty() || (target && target->x == _pos.x && target->y == _pos.y))
	{
		error = "STR_VEHICLE_ALREADY_THERE";
		return false;
	}
	const Mod *mod = save->getMod();
	const int crushArmor = getCrushArmor(save->getMod());
	const std::vector<BattleUnit*> passengers = getPassengers(save);
	std::vector<char> inside(_cells.size(), 0);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		inside[i] = isOnBoard(save, _pos + _cells[i].offset);
	}
	const int width = save->getMapSizeX(), length = save->getMapSizeY(), height = save->getMapSizeZ();

	// running over is the player's choice: only the units under the place clicked
	std::vector<const BattleUnit*> targetUnits;
	for (const auto &column : _columns)
	{
		for (int z = 0; z < height && target; ++z)
		{
			const Tile *tile = save->getTile(Position(target->x + column.x, target->y + column.y, z));
			if (tile && tile->getUnit())
			{
				targetUnits.push_back(tile->getUnit());
			}
		}
	}
	auto judge = [&](const BattleUnit *unit)
	{
		if (!knowsOf(unit))
		{
			return UNIT_IGNORED;
		}
		const UnitInWay inWay = judgeUnit(save, unit);
		if (inWay == UNIT_RUN_OVER && _faction == FACTION_PLAYER && std::find(targetUnits.begin(), targetUnits.end(), unit) == targetUnits.end())
		{
			return UNIT_BLOCKS;
		}
		return inWay;
	};

	// where the vehicle stands with its corner at (x, y), coming from a level with some columns in the ground above it:
	// its level (-1 if it doesn't fit there), the number of parts it crushes and units it runs over, and what blocks it
	struct Fit
	{
		int z, crushed, victims;
		MoveResult blocked;
	};
	std::unordered_map<long long, Fit> fits;
	auto fitsAt = [&](int x, int y, int fromZ, int fromBuried)
	{
		Fit fit = { -1, 0, 0, MOVE_BLOCKED_EDGE };
		if (x < 0 || y < 0 || x >= width || y >= length)
		{
			return fit;
		}
		const long long key = (((long long)fromBuried * height + fromZ) * length + y) * width + x;
		auto known = fits.find(key);
		if (known != fits.end())
		{
			return known->second;
		}
		const bool onMap = std::all_of(_columns.begin(), _columns.end(), [&](Position column) { return x + column.x < width && y + column.y < length; });
		if (onMap)
		{
			fit.z = findLevel(save, x, y, fromZ, fromBuried);
			fit.blocked = MOVE_TOO_STEEP;
		}
		if (fit.z >= 0)
		{
			std::vector<std::pair<Tile*, TilePart> > toCrush;
			std::vector<BattleUnit*> victims;
			fit.blocked = checkPlace(save, Position(x, y, fit.z), inside, passengers, crushArmor, judge, toCrush, victims);
			if (fit.blocked == MOVE_OK)
			{
				fit.crushed = (int)toCrush.size();
				fit.victims = (int)victims.size();
			}
			else
			{
				fit.z = -1;
			}
		}
		fits[key] = fit;
		return fit;
	};
	// the target, coming from any level; when it doesn't fit, what stands in the way says more than the ground
	MoveResult atTarget = target ? MOVE_TOO_STEEP : MOVE_OK;
	for (int z = 0; z < height && atTarget != MOVE_OK; ++z)
	{
		const MoveResult result = fitsAt(target->x, target->y, z, (int)_columns.size()).blocked;
		if (result == MOVE_OK || atTarget == MOVE_TOO_STEEP)
		{
			atTarget = result;
		}
	}
	if (atTarget != MOVE_OK)
	{
		error = getMoveResultString(atTarget);
		return false;
	}

	auto estimate = [&](int x, int y)
	{
		if (!target)
		{
			return 0;
		}
		const int dx = std::abs(x - target->x), dy = std::abs(y - target->y);
		return 2 * std::max(dx, dy) + std::min(dx, dy);
	};
	auto nodeAt = [&](int x, int y, int z)
	{
		return (z * length + y) * width + x;
	};
	const int start = nodeAt(_pos.x, _pos.y, _pos.z);
	int goal = -1;
	std::unordered_map<int, int> cost, from, buried, tuSpent, crushing;
	using Entry = std::pair<int, int>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry> > open;
	cost[start] = 0;
	buried[start] = countBuried(save, _pos);
	tuSpent[start] = 0;
	crushing[start] = 0;
	open.push(std::make_pair(estimate(_pos.x, _pos.y), start));
	int expanded = 0;
	while (!open.empty() && expanded < 20000)
	{
		const Entry top = open.top();
		open.pop();
		const int current = top.second;
		const int cx = current % width, cy = current / width % length, cz = current / (width * length);
		if (target && cx == target->x && cy == target->y)
		{
			goal = current;
			break;
		}
		if (top.first - estimate(cx, cy) > cost[current])
		{
			continue; // a better way to this position was found after this entry
		}
		++expanded;
		const int currentBuried = buried[current];
		for (int dir = 0; dir < 8; ++dir)
		{
			Position v;
			Pathfinding::directionToVector(dir, &v);
			const Fit fit = fitsAt(cx + v.x, cy + v.y, cz, currentBuried);
			const bool diagonal = v.x != 0 && v.y != 0;
			if (fit.z < 0 || (diagonal && (fitsAt(cx + v.x, cy, cz, currentBuried).z < 0 || fitsAt(cx, cy + v.y, cz, currentBuried).z < 0)))
			{
				continue;
			}
			const int next = nodeAt(cx + v.x, cy + v.y, fit.z);
			const int tu = tuSpent[current] + getStepCost(mod, Position(v.x, v.y, fit.z - cz));
			if (!target && tu > budget)
			{
				continue;
			}
			int step = diagonal ? 3 : 2;
			if (fit.z > cz)
			{
				step = step * 3 / 2;
			}
			const int newCost = cost[current] + step + (fit.crushed > 0 ? 2 : 0) + (fit.victims > 0 ? 6 : 0);
			auto known = cost.find(next);
			if (known == cost.end() || newCost < known->second)
			{
				if (known == cost.end())
				{
					buried[next] = countBuried(save, Position(cx + v.x, cy + v.y, fit.z));
				}
				cost[next] = newCost;
				from[next] = current;
				tuSpent[next] = tu;
				crushing[next] = crushing[current] + (fit.crushed > 0 || fit.victims > 0 ? 1 : 0);
				open.push(std::make_pair(newCost + estimate(cx + v.x, cy + v.y), next));
			}
		}
	}
	if (!target)
	{
		for (const auto &place : cost)
		{
			const int at = place.first;
			reach->push_back({ Position(at % width, at / width % length, at / (width * length)), tuSpent[at], crushing[at] });
		}
		return false;
	}
	if (goal == -1)
	{
		error = "STR_VEHICLE_NO_ROUTE";
		return false;
	}
	for (int at = goal; at != start; at = from[at])
	{
		path.push_back(Position(at % width, at / width % length, at / (width * length)));
	}
	std::reverse(path.begin(), path.end());
	return true;
}

/**
 * Finds the steps that take the vehicle to a new place (see search).
 * @param save Battle save.
 * @param target New vehicle position (its level is ignored).
 * @param path Gets the vehicle positions after each step, the last one being the target.
 * @param error Gets a string ID telling why there is no way.
 * @return True if a way was found.
 */
bool BattleVehicle::findPath(SavedBattleGame *save, Position target, std::vector<Position> &path, std::string &error) const
{
	return search(save, &target, 0, path, nullptr, error);
}

/**
 * Finds every place the vehicle can drive to with some time units, its current place included
 * (see search).
 * @param save Battle save.
 * @param tu Time units the vehicle may spend.
 * @return The places, with what it takes to get to each.
 */
std::vector<BattleVehicle::Reach> BattleVehicle::findReachable(SavedBattleGame *save, int tu) const
{
	std::vector<Reach> reach;
	std::vector<Position> path;
	std::string error;
	search(save, nullptr, tu, path, &reach, error);
	return reach;
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
	case MOVE_TOO_STEEP:
		return "STR_VEHICLE_TOO_STEEP";
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
