/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tbtr_template_vehicle_func.cpp Template-based train replacement: template vehicle functions. */

#include "stdafx.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "window_func.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "train.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "core/geometry_type.hpp"
#include "debug.h"
#include "zoom_func.h"
#include "core/backup_type.hpp"
#include "core/random_func.hpp"

#include "table/sprites.h"
#include "table/strings.h"

#include "cargoaction.h"
#include "train.h"
#include "company_func.h"
#include "newgrf.h"
#include "spritecache.h"
#include "articulated_vehicles.h"
#include "autoreplace_func.h"

#include "depot_base.h"

#include "tbtr_template_vehicle.h"
#include "tbtr_template_vehicle_func.h"

#include <map>
#include <stdio.h>

#include "safeguards.h"

bool _template_vehicle_images_valid = false;

#ifdef _DEBUG
// debugging printing functions for convenience, usually called from gdb
void tbtr_debug_pat()
{
	for (TemplateVehicle *tv : TemplateVehicle::Iterate()) {
		if (tv->Prev()) continue;
		tbtr_debug_ptv(tv);
		printf("__________\n");
	}
}

void tbtr_debug_pav()
{
	for (Train *t : Train::Iterate()) {
		if (t->Previous()) continue;
		tbtr_debug_pvt(t);
		printf("__________\n");
	}
}

void tbtr_debug_ptv(TemplateVehicle* tv)
{
	if (!tv) return;
	while (tv->Next() ) {
		printf("eid:%3d  st:%2d  tv:%p  next:%p  cargo: %d  cargo_sub: %d\n", tv->engine_type, tv->subtype, tv, tv->Next(), tv->cargo_type, tv->cargo_subtype);
		tv = tv->Next();
	}
	printf("eid:%3d  st:%2d  tv:%p  next:%p  cargo: %d  cargo_sub: %d\n", tv->engine_type, tv->subtype, tv, tv->Next(),  tv->cargo_type, tv->cargo_subtype);
}

void tbtr_debug_pvt (const Train *printme)
{
	for (const Train *tmp = printme; tmp; tmp = tmp->Next()) {
		if (tmp->index <= 0) {
			printf("train has weird index: %d %d %p\n", tmp->index, tmp->engine_type, tmp);
			return;
		}
		printf("eid:%3d  index:%2d  subtype:%2d  vehstat: %d  cargo_t: %d   cargo_sub: %d  ref:%p\n", tmp->engine_type, tmp->index, tmp->subtype, tmp->vehstatus, tmp->cargo_type, tmp->cargo_subtype, tmp);
	}
}
#endif

void BuildTemplateGuiList(GUITemplateList *list, Scrollbar *vscroll, Owner oid, RailType railtype)
{
	list->clear();
	for (const TemplateVehicle *tv : TemplateVehicle::Iterate()) {
		if (tv->owner == oid && (tv->IsPrimaryVehicle() || tv->IsFreeWagonChain()) && TemplateVehicleContainsEngineOfRailtype(tv, railtype)) {
			list->push_back(tv);
		}
	}

	list->RebuildDone();
	if (vscroll) vscroll->SetCount((uint)list->size());
}

Money CalculateOverallTemplateCost(const TemplateVehicle *tv)
{
	Money val = 0;

	for (; tv; tv = tv->GetNextUnit()) {
		val += (Engine::Get(tv->engine_type))->GetCost();
	}
	return val;
}

Money CalculateOverallTemplateDisplayRunningCost(const TemplateVehicle *tv)
{
	Money val = 0;

	for (; tv; tv = tv->GetNextUnit()) {
		val += (Engine::Get(tv->engine_type))->GetDisplayRunningCost();
	}
	return val;
}

void DrawTemplate(const TemplateVehicle *tv, int left, int right, int y)
{
	if (!tv) return;

	DrawPixelInfo tmp_dpi, *old_dpi;
	int max_width = right - left + 1;
	int height = ScaleGUITrad(14);
	int padding = ScaleGUITrad(1);
	if (!FillDrawPixelInfo(&tmp_dpi, left, y - padding, max_width, height + (2 * padding))) return;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	const TemplateVehicle *t = tv;
	int offset = 0;

	while (t) {
		t->sprite_seq.Draw(offset + t->image_dimensions.GetOffsetX(), t->image_dimensions.GetOffsetY() + ScaleGUITrad(10), t->colourmap, false);

		offset += t->image_dimensions.GetDisplayImageWidth();
		t = t->Next();
	}

	_cur_dpi = old_dpi;
}

// copy important stuff from the virtual vehicle to the template
void SetupTemplateVehicleFromVirtual(TemplateVehicle *tmp, TemplateVehicle *prev, Train *virt)
{
	if (prev) {
		prev->SetNext(tmp);
		tmp->SetPrev(prev);
		tmp->SetFirst(prev->First());
	}
	tmp->railtype = virt->railtype;
	tmp->owner = virt->owner;

	// set the subtype but also clear the virtual flag while doing it
	tmp->subtype = virt->subtype & ~(1 << GVSF_VIRTUAL);
	// set the cargo type and capacity
	tmp->cargo_type = virt->cargo_type;
	tmp->cargo_subtype = virt->cargo_subtype;
	tmp->cargo_cap = virt->cargo_cap;

	SB(tmp->ctrl_flags, TVCF_REVERSED, 1, HasBit(virt->flags, VRF_REVERSE_DIRECTION) ? 1 : 0);

	if (!virt->Previous()) {
		uint cargo_weight = 0;
		uint full_cargo_weight = 0;
		for (const Train *u = virt; u != nullptr; u = u->Next()) {
			cargo_weight += u->GetCargoWeight(u->cargo.StoredCount());
			full_cargo_weight += u->GetCargoWeight(u->cargo_cap);
		}
		const GroundVehicleCache *gcache = virt->GetGroundVehicleCache();
		tmp->max_speed = virt->GetDisplayMaxSpeed();
		tmp->power = gcache->cached_power;
		tmp->empty_weight = std::max<uint32>(gcache->cached_weight - cargo_weight, 1);
		tmp->full_weight = std::max<uint32>(gcache->cached_weight + full_cargo_weight - cargo_weight, 1);
		tmp->max_te = gcache->cached_max_te;
		tmp->air_drag = gcache->cached_air_drag;
	}

	virt->GetImage(DIR_W, EIT_IN_DEPOT, &tmp->sprite_seq);
	tmp->image_dimensions.SetFromTrain(virt);
	tmp->colourmap = GetUncachedTrainPaletteIgnoringGroup(virt);
}

// create a full TemplateVehicle based train according to a virtual train
TemplateVehicle* TemplateVehicleFromVirtualTrain(Train *virt)
{
	assert(virt != nullptr);

	Train *init_virt = virt;

	TemplateVehicle *tmp;
	TemplateVehicle *prev = nullptr;
	for (; virt; virt = virt->Next()) {
		tmp = new TemplateVehicle(virt->engine_type);
		SetupTemplateVehicleFromVirtual(tmp, prev, virt);
		prev = tmp;
	}

	tmp->First()->SetRealLength(CeilDiv(init_virt->gcache.cached_total_length * 10, TILE_SIZE));
	return tmp->First();
}

// forward declaration, defined in train_cmd.cpp
CommandCost CmdSellRailWagon(DoCommandFlag, Vehicle*, uint16, uint32);

Train* DeleteVirtualTrain(Train *chain, Train *to_del) {
	if (chain != to_del) {
		CmdSellRailWagon(DC_EXEC, to_del, 0, 0);
		return chain;
	}
	else {
		chain = chain->GetNextUnit();
		CmdSellRailWagon(DC_EXEC, to_del, 0, 0);
		return chain;
	}
}

// retrieve template vehicle from template replacement that belongs to the given group
TemplateVehicle* GetTemplateVehicleByGroupID(GroupID gid) {
	if (gid >= NEW_GROUP) return nullptr;
	const TemplateID tid = GetTemplateIDByGroupID(gid);
	return tid != INVALID_TEMPLATE ? TemplateVehicle::GetIfValid(tid) : nullptr;
}

TemplateVehicle* GetTemplateVehicleByGroupIDRecursive(GroupID gid) {
	if (gid >= NEW_GROUP) return nullptr;
	const TemplateID tid = GetTemplateIDByGroupIDRecursive(gid);
	return tid != INVALID_TEMPLATE ? TemplateVehicle::GetIfValid(tid) : nullptr;
}

/**
 * Check a template consist whether it contains any engine of the given railtype
 */
bool TemplateVehicleContainsEngineOfRailtype(const TemplateVehicle *tv, RailType type)
{
	if (type == INVALID_RAILTYPE) return true;

	/* For standard rail engines, allow only those */
	if (type == RAILTYPE_BEGIN || type == RAILTYPE_RAIL) {
		while (tv) {
			if (tv->railtype != type) {
				return false;
			}
			tv = tv->GetNextUnit();
		}
		return true;
	}
	/* For electrified rail engines, standard wagons or engines are allowed to be included */
	while (tv) {
		if (tv->railtype == type) {
			return true;
		}
		tv = tv->GetNextUnit();
	}
	return false;
}

//helper
bool ChainContainsVehicle(Train *chain, Train *mem)
{
	for (; chain; chain = chain->Next()) {
		if (chain == mem) {
			return true;
		}
	}
	return false;
}

// has O(n)
Train* ChainContainsEngine(EngineID eid, Train *chain) {
	for (; chain; chain=chain->GetNextUnit())
		if (chain->engine_type == eid)
			return chain;
	return nullptr;
}

static bool IsTrainUsableAsTemplateReplacementSource(const Train *t)
{
	if (t->IsFreeWagon()) return true;

	if (t->IsPrimaryVehicle() && t->IsStoppedInDepot()) {
		if (t->GetNumOrders() != 0) return false;
		if (t->IsOrderListShared()) return false;
		if (t->group_id != DEFAULT_GROUP) return false;
		return true;
	}

	return false;
}

// has O(n^2)
Train* DepotContainsEngine(TileIndex tile, EngineID eid, Train *not_in = nullptr)
{
	for (Train *t : Train::Iterate()) {
		// conditions: v is stopped in the given depot, has the right engine and if 'not_in' is given v must not be contained within 'not_in'
		// if 'not_in' is nullptr, no check is needed
		if (t->tile == tile
				// If the veh belongs to a chain, wagons will not return true on IsStoppedInDepot(), only primary vehicles will
				// in case of t not a primary veh, we demand it to be a free wagon to consider it for replacement
				&& IsTrainUsableAsTemplateReplacementSource(t)
				&& t->engine_type == eid
				&& (not_in == nullptr || ChainContainsVehicle(not_in, t) == false)) {
			return t;
		}
	}
	return nullptr;
}

void NeutralizeStatus(Train *t)
{
	DoCommand(t->tile, DEFAULT_GROUP, t->index, DC_EXEC, CMD_ADD_VEHICLE_GROUP);
	DoCommand(0, t->index | CO_UNSHARE << 30, 0, DC_EXEC, CMD_CLONE_ORDER);
	DoCommand(0, t->index, FreeUnitIDGenerator(VEH_TRAIN, t->owner).NextID(), DC_EXEC, CMD_SET_VEHICLE_UNIT_NUMBER);
	DoCommand(0, t->index, 0, DC_EXEC, CMD_RENAME_VEHICLE, nullptr);
}

bool TrainMatchesTemplate(const Train *t, const TemplateVehicle *tv) {
	while (t && tv) {
		if (t->engine_type != tv->engine_type) {
			return false;
		}
		t = t->GetNextUnit();
		tv = tv->GetNextUnit();
	}
	if ((t && !tv) || (!t && tv)) {
		return false;
	}
	return true;
}


bool TrainMatchesTemplateRefit(const Train *t, const TemplateVehicle *tv)
{
	if (!tv->refit_as_template) {
		return true;
	}

	while (t && tv) {
		if (t->cargo_type != tv->cargo_type || t->cargo_subtype != tv->cargo_subtype || HasBit(t->flags, VRF_REVERSE_DIRECTION) != HasBit(tv->ctrl_flags, TVCF_REVERSED)) {
			return false;
		}
		t = t->GetNextUnit();
		tv = tv->GetNextUnit();
	}
	return true;
}

void BreakUpRemainders(Train *t)
{
	while (t) {
		Train *move;
		if (HasBit(t->subtype, GVSF_ENGINE)) {
			move = t;
			t = t->Next();
			DoCommand(move->tile, move->index | (1 << 22), INVALID_VEHICLE, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			NeutralizeStatus(move);
		} else {
			t = t->Next();
		}
	}
}

// make sure the real train wagon has the right cargo
void CopyWagonStatus(TemplateVehicle *from, Train *to)
{
	to->cargo_type = from->cargo_type;
	to->cargo_subtype = from->cargo_subtype;
}

int NumTrainsNeedTemplateReplacement(GroupID g_id, const TemplateVehicle *tv)
{
	int count = 0;
	if (!tv) return count;

	for (Train *t : Train::Iterate()) {
		if (t->IsPrimaryVehicle() && t->group_id == g_id && (!TrainMatchesTemplate(t, tv) || !TrainMatchesTemplateRefit(t, tv))) {
			count++;
		}
	}
	return count;
}
// refit each vehicle in t as is in tv, assume t and tv contain the same types of vehicles
CommandCost CmdRefitTrainFromTemplate(Train *t, TemplateVehicle *tv, DoCommandFlag flags)
{
	CommandCost cost(t->GetExpenseType(false));

	while (t && tv) {
		// refit t as tv
		uint32 cb = GetCmdRefitVeh(t);

		cost.AddCost(DoCommand(t->tile, t->index, tv->cargo_type | tv->cargo_subtype << 8 | (1 << 16) | (1 << 31), flags, cb));

		if (HasBit(t->flags, VRF_REVERSE_DIRECTION) != HasBit(tv->ctrl_flags, TVCF_REVERSED)) {
			cost.AddCost(DoCommand(t->tile, t->index, true, flags, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_ERROR_CAN_T_REVERSE_DIRECTION_RAIL_VEHICLE)));
		}

		// next
		t = t->GetNextUnit();
		tv = tv->GetNextUnit();
	}
	return cost;
}

/** using cmdtemplatereplacevehicle as test-function (i.e. with flag DC_NONE) is not a good idea as that function relies on
 *  actually moving vehicles around to work properly.
 *  We do this worst-cast test instead.
 */
CommandCost TestBuyAllTemplateVehiclesInChain(TemplateVehicle *tv, TileIndex tile)
{
	CommandCost cost(EXPENSES_NEW_VEHICLES);

	for (; tv; tv = tv->GetNextUnit()) {
		cost.AddCost(DoCommand(tile, tv->engine_type, 0, DC_NONE, CMD_BUILD_VEHICLE));
	}

	return cost;
}


/** Transfer as much cargo from a given (single train) vehicle onto a chain of vehicles.
 *  I.e., iterate over the chain from head to tail and use all available cargo capacity (w.r.t. cargo type of course)
 *  to store the cargo from the given single vehicle.
 *  @param old_veh:     ptr to the single vehicle, which's cargo shall be moved
 *  @param new_head:    ptr to the head of the chain, which shall obtain old_veh's cargo
 *  @return:            amount of moved cargo, TODO
 */
void TransferCargoForTrain(Train *old_veh, Train *new_head)
{
	assert(new_head->IsPrimaryVehicle() || new_head->IsFreeWagon());

	CargoID _cargo_type = old_veh->cargo_type;
	byte _cargo_subtype = old_veh->cargo_subtype;

	// how much cargo has to be moved (if possible)
	uint remainingAmount = old_veh->cargo.TotalCount();
	// each vehicle in the new chain shall be given as much of the old cargo as possible, until none is left
	for (Train *tmp = new_head; tmp != nullptr && remainingAmount > 0; tmp = tmp->GetNextUnit()) {
		if (tmp->cargo_type == _cargo_type && tmp->cargo_subtype == _cargo_subtype) {
			// calculate the free space for new cargo on the current vehicle
			uint curCap = tmp->cargo_cap - tmp->cargo.TotalCount();
			uint moveAmount = std::min(remainingAmount, curCap);
			// move (parts of) the old vehicle's cargo onto the current vehicle of the new chain
			if (moveAmount > 0) {
				old_veh->cargo.Shift(moveAmount, &tmp->cargo);
				remainingAmount -= moveAmount;
			}
		}
	}

	// TODO: needs to be implemented, too
	// // from autoreplace_cmd.cpp : 121
	/* Any left-overs will be thrown away, but not their feeder share. */
	//if (src->cargo_cap < src->cargo.TotalCount()) src->cargo.Truncate(src->cargo.TotalCount() - src->cargo_cap);

	/* Update train weight etc., the old vehicle will be sold anyway */
	new_head->ConsistChanged(CCF_LOADUNLOAD);
}

void UpdateAllTemplateVehicleImages()
{
	SavedRandomSeeds saved_seeds;
	SaveRandomSeeds(&saved_seeds);

	for (TemplateVehicle *tv : TemplateVehicle::Iterate()) {
		if (tv->Prev() == nullptr) {
			Backup<CompanyID> cur_company(_current_company, tv->owner, FILE_LINE);
			StringID err;
			Train* t = VirtualTrainFromTemplateVehicle(tv, err, 0);
			if (t != nullptr) {
				int tv_len = 0;
				for (TemplateVehicle *u = tv; u != nullptr; u = u->Next()) {
					tv_len++;
				}
				int t_len = 0;
				for (Train *u = t; u != nullptr; u = u->Next()) {
					t_len++;
				}
				if (t_len == tv_len) {
					Train *v = t;
					for (TemplateVehicle *u = tv; u != nullptr; u = u->Next(), v = v->Next()) {
						v->GetImage(DIR_W, EIT_IN_DEPOT, &u->sprite_seq);
						u->image_dimensions.SetFromTrain(v);
						u->colourmap = GetVehiclePalette(v);
					}
				} else {
					DEBUG(misc, 0, "UpdateAllTemplateVehicleImages: vehicle count mismatch: %u, %u", t_len, tv_len);
				}
				delete t;
			}
			cur_company.Restore();
		}
	}

	RestoreRandomSeeds(saved_seeds);

	_template_vehicle_images_valid = true;
}

int GetTemplateVehicleEstimatedMaxAchievableSpeed(const TemplateVehicle *tv, const int mass, const int speed_cap)
{
	int max_speed = 0;
	int acceleration;

	do
	{
		max_speed++;
		acceleration = GetTrainRealisticAccelerationAtSpeed(max_speed, mass, tv->power, tv->max_te, tv->air_drag, tv->railtype);
	} while (acceleration > 0 && max_speed < speed_cap);

	return max_speed;
}
