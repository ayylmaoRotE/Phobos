#include "Body.h"

#include <BitFont.h>

#include <Utilities/EnumFunctions.h>

BuildingExt::ExtContainer BuildingExt::ExtMap;

void BuildingExt::ExtData::DisplayIncomeString()
{
	if (this->AccumulatedIncome && Unsorted::CurrentFrame % 15 == 0)
	{
		auto const pOwnerObject = this->OwnerObject();
		auto const pTypeExt = this->TypeExtData;

		if ((RulesExt::Global()->DisplayIncome_AllowAI || pOwnerObject->Owner->IsControlledByHuman())
			&& pTypeExt->DisplayIncome.Get(RulesExt::Global()->DisplayIncome))
		{
			FlyingStrings::AddMoneyString(
				this->AccumulatedIncome,
				pOwnerObject->Owner,
				pTypeExt->DisplayIncome_Houses.Get(RulesExt::Global()->DisplayIncome_Houses.Get()),
				pOwnerObject->GetRenderCoords(),
				pTypeExt->DisplayIncome_Offset
			);
		}
		this->AccumulatedIncome = 0;
	}
}

bool BuildingExt::ExtData::HasSuperWeapon(const int index, const bool withUpgrades) const
{
	const auto pThis = this->OwnerObject();
	const auto pExt = BuildingTypeExt::ExtMap.Find(pThis->Type);
	const auto pOwner = pThis->Owner;

	const auto count = pExt->GetSuperWeaponCount();
	for (auto i = 0; i < count; ++i)
	{
		const auto idxSW = pExt->GetSuperWeaponIndex(i, pOwner);

		if (idxSW == index)
			return true;
	}

	if (withUpgrades)
	{
		for (auto const& pUpgrade : pThis->Upgrades)
		{
			if (const auto pUpgradeExt = BuildingTypeExt::ExtMap.TryFind(pUpgrade))
			{
				const auto countUpgrade = pUpgradeExt->GetSuperWeaponCount();
				for (auto i = 0; i < countUpgrade; ++i)
				{
					const auto idxSW = pUpgradeExt->GetSuperWeaponIndex(i, pOwner);

					if (idxSW == index)
						return true;
				}
			}
		}
	}

	return false;
}

void BuildingExt::StoreTiberium(BuildingClass* pThis, float amount, int idxTiberiumType, int idxStorageTiberiumType)
{
	auto const pDepositableTiberium = TiberiumClass::Array.GetItem(idxStorageTiberiumType);
	float depositableTiberiumAmount = 0.0f; // Number of 'bails' that will be stored.
	auto const pTiberium = TiberiumClass::Array.GetItem(idxTiberiumType);

	if (amount > 0.0)
	{
		auto const pExt = BuildingTypeExt::ExtMap.Find(pThis->Type);

		if (pExt->Refinery_UseStorage)
		{
			// Store Tiberium in structures
			depositableTiberiumAmount = (amount * pTiberium->Value) / pDepositableTiberium->Value;
			pThis->Owner->GiveTiberium(depositableTiberiumAmount, idxStorageTiberiumType);
		}
	}
}

void BuildingExt::ExtData::UpdatePrimaryFactoryAI()
{
	// Owner & current production type
	auto* const pOwnerObj = this->OwnerObject();
	HouseClass* const owner = pOwnerObj ? pOwnerObj->Owner : nullptr;
	if (!owner) return;

	const int prodIdx = owner->ProducingAircraftTypeIndex;
	if (prodIdx < 0) return;

	auto* const pAircraft = AircraftTypeClass::Array.GetItem(prodIdx);
	FactoryClass* currFactory = owner->GetFactoryProducing(pAircraft);

	// Prefer the previously chosen air-factory if it still has free docks
	BuildingClass* currentAir = this->CurrentAirFactory;
	if (currentAir)
	{
		const BuildingTypeClass* const ft = currentAir->Type;
		const int nDocks = ft ? ft->NumberOfDocks : 0;
		const int nOcc = BuildingExt::CountOccupiedDocks(currentAir);

		if (nOcc < nDocks)
		{
			currFactory = currentAir->Factory;
		}
		else
		{
			this->CurrentAirFactory = nullptr; // no longer valid
			currentAir = nullptr;
		}
	}

	// Collect all air-factory buildings; also pick a fallback currFactory if still null
	std::vector<BuildingClass*> airFactories;
	airFactories.reserve(owner->Buildings.Count); // avoids growth
	for (auto* const b : owner->Buildings)
	{
		const BuildingTypeClass* const bt = b->Type;
		if (bt->Factory != AbstractType::AircraftType)
			continue;

		if (!currFactory && b->Factory)
			currFactory = b->Factory;

		airFactories.emplace_back(b);
	}

	// If we already have a valid "current" air factory: mark it primary, others non-primary
	if (currentAir)
	{
		for (auto* const b : airFactories)
		{
			if (b == currentAir)
			{
				currentAir->Factory = currFactory; // keep producing with the same factory pointer
				currentAir->IsPrimaryFactory = true;
			}
			else
			{
				b->IsPrimaryFactory = false;
				if (b->Factory)
				{
					b->Factory->AbandonProduction();
				}
			}
		}
		return;
	}

	// No current air-factory and no factory to drive production: nothing to do
	if (!currFactory) return;

	// Choose the first building with a free dock; mark it primary, cancel others
	BuildingClass* chosen = nullptr;
	for (auto* const b : airFactories)
	{
		if (!chosen)
		{
			const int docks = b->Type->NumberOfDocks;
			if (BuildingExt::CountOccupiedDocks(b) < docks)
			{
				chosen = b;
				chosen->Factory = currFactory;
				chosen->IsPrimaryFactory = true;
				this->CurrentAirFactory = chosen;
				continue;
			}
		}

		b->IsPrimaryFactory = false;
		if (b->Factory)
		{
			b->Factory->AbandonProduction();
		}
	}
}

int BuildingExt::CountOccupiedDocks(BuildingClass* pBuilding)
{
	if (!pBuilding)
		return 0;

	int nOccupiedDocks = 0;

	if (pBuilding->RadioLinks.IsAllocated)
	{
		for (auto i = 0; i < pBuilding->RadioLinks.Capacity; ++i)
		{
			if (auto const pLink = pBuilding->GetNthLink(i))
				nOccupiedDocks++;
		}
	}

	return nOccupiedDocks;
}

bool BuildingExt::HasFreeDocks(BuildingClass* pBuilding)
{
	auto const pType = pBuilding->Type;

	if (pType->Factory == AbstractType::AircraftType)
	{
		const int nDocks = pType->NumberOfDocks;
		const int nOccupiedDocks = BuildingExt::CountOccupiedDocks(pBuilding);

		if (nOccupiedDocks < nDocks)
			return true;
		else
			return false;
	}

	return false;
}

bool BuildingExt::CanGrindTechno(BuildingClass* pBuilding, TechnoClass* pTechno)
{
	auto const pBldType = pBuilding->Type;
	auto const whatAmI = pTechno->WhatAmI();

	if (!pBldType->Grinding || (whatAmI != AbstractType::Infantry && whatAmI != AbstractType::Unit))
		return false;

	if ((pBldType->InfantryAbsorb || pBldType->UnitAbsorb)
		&& (whatAmI == AbstractType::Infantry && !pBldType->InfantryAbsorb
			|| whatAmI == AbstractType::Unit && !pBldType->UnitAbsorb))
	{
		return false;
	}

	auto const pExt = BuildingTypeExt::ExtMap.Find(pBldType);

	if (pBuilding->Owner == pTechno->Owner && !pExt->Grinding_AllowOwner)
		return false;

	if (pBuilding->Owner != pTechno->Owner && pBuilding->Owner->IsAlliedWith(pTechno) && !pExt->Grinding_AllowAllies)
		return false;

	auto const pType = pTechno->GetTechnoType();
	auto const& allowTypes = pExt->Grinding_AllowTypes;
	auto const& disallowTypes = pExt->Grinding_DisallowTypes;

	if (allowTypes.size() > 0 && !allowTypes.Contains(pType))
		return false;

	if (disallowTypes.size() > 0 && disallowTypes.Contains(pType))
		return false;


	return true;
}

bool BuildingExt::DoGrindingExtras(BuildingClass* pBuilding, TechnoClass* pTechno, int refund)
{
	if (auto const pExt = BuildingExt::ExtMap.TryFind(pBuilding))
	{
		auto const pTypeExt = pExt->TypeExtData;

		pExt->AccumulatedIncome += refund;
		pExt->GrindingWeapon_AccumulatedCredits += refund;

		if (pTypeExt->Grinding_Weapon
			&& Unsorted::CurrentFrame >= pExt->GrindingWeapon_LastFiredFrame + pTypeExt->Grinding_Weapon->ROF
			&& pExt->GrindingWeapon_AccumulatedCredits >= pTypeExt->Grinding_Weapon_RequiredCredits)
		{
			TechnoExt::FireWeaponAtSelf(pBuilding, pTypeExt->Grinding_Weapon);
			pExt->GrindingWeapon_LastFiredFrame = Unsorted::CurrentFrame;
			pExt->GrindingWeapon_AccumulatedCredits = 0;
		}

		if (pTypeExt->Grinding_Sound >= 0)
		{
			VocClass::PlayAt(pTypeExt->Grinding_Sound, pTechno->GetCoords());
			return true;
		}
	}

	return false;
}

// Building only or allow units too?
void BuildingExt::ExtData::ApplyPoweredKillSpawns()
{
	auto const pThis = this->OwnerObject();
	auto const pTypeExt = this->TypeExtData;

	if (pTypeExt->Powered_KillSpawns && pThis->Type->Powered && !pThis->IsPowerOnline())
	{
		if (auto const pManager = pThis->SpawnManager)
		{
			pManager->ResetTarget();
			for (auto const pItem : pManager->SpawnedNodes)
			{
				auto const status = pItem->Status;
				if (status == SpawnNodeStatus::Attacking || status == SpawnNodeStatus::Returning)
				{
					auto const pUnit = pItem->Unit;
					pUnit->ReceiveDamage(&pUnit->Health, 0, RulesClass::Instance->C4Warhead, nullptr, true, false, nullptr);
				}
			}
		}
	}
}

bool BuildingExt::ExtData::HandleInfiltrate(HouseClass* pInfiltratorHouse, int moneybefore)
{
	const auto pVictimHouse = this->OwnerObject()->Owner;
	const auto pTypeExt = this->TypeExtData;
	this->AccumulatedIncome += pVictimHouse->Available_Money() - moneybefore;

	if (!pVictimHouse->IsControlledByHuman() && !RulesExt::Global()->DisplayIncome_AllowAI)
	{
		// TODO there should be a better way...
		FlyingStrings::AddMoneyString(
				this->AccumulatedIncome,
				pVictimHouse,
				pTypeExt->DisplayIncome_Houses.Get(RulesExt::Global()->DisplayIncome_Houses.Get()),
				this->OwnerObject()->GetRenderCoords(),
				pTypeExt->DisplayIncome_Offset
		);
	}

	if (!pTypeExt->SpyEffect_Custom)
		return false;

	if (pInfiltratorHouse != pVictimHouse)
	{
		// I assume you were not launching for real, Morton

		auto launchTheSWHere = [this](SuperClass* const pSuper, HouseClass* const pHouse)->void
			{
				const int oldstart = pSuper->RechargeTimer.StartTime;
				const int oldleft = pSuper->RechargeTimer.TimeLeft;
				pSuper->SetReadiness(true);
				pSuper->Launch(CellClass::Coord2Cell(this->OwnerObject()->GetCenterCoords()), pHouse->IsCurrentPlayer());
				pSuper->Reset();
				pSuper->RechargeTimer.StartTime = oldstart;
				pSuper->RechargeTimer.TimeLeft = oldleft;
			};

		const int idx1 = pTypeExt->SpyEffect_VictimSuperWeapon;
		if (idx1 >= 0)
			launchTheSWHere(pVictimHouse->Supers.Items[idx1], pVictimHouse);

		const int idx2 = pTypeExt->SpyEffect_InfiltratorSuperWeapon;
		if (idx2 >= 0)
			launchTheSWHere(pInfiltratorHouse->Supers.Items[idx2], pInfiltratorHouse);
	}

	return true;
}

// For unit's weapons factory only
void BuildingExt::KickOutStuckUnits(BuildingClass* pThis)
{
	// 1) If the factory currently has a linked unit and it's stuck, free & send it to Guard.
	if (auto* const pUnit = abstract_cast<UnitClass*>(pThis->GetNthLink()))
	{
		if (!pUnit->IsTether && pUnit->GetCurrentSpeed() <= 0)
		{
			if (auto* const pTeam = pUnit->Team)
				pTeam->LiberateMember(pUnit);

			pThis->SendCommand(RadioCommand::NotifyUnlink, pUnit);
			pUnit->QueueMission(Mission::Guard, false);
			return; // one-after-another
		}
	}

	// 2) Otherwise, scan the exit cell (and one neighbor to the East) for a same-house, non-tethered unit
	//    that is within cell height. Link it and force an Unload.
	CoordStruct buffer = CoordStruct::Empty;
	CellClass* cell = MapClass::Instance.GetCellAt(*pThis->GetExitCoords(&buffer, 0));
	if (!cell)
		return;

	HouseClass* const pOwner = pThis->Owner;

	// Check up to 2 cells: exit cell, then East neighbor.
	for (int step = 0; step < 2 && cell; ++step)
	{
		for (auto* obj = cell->FirstObject; obj; obj = obj->NextObject)
		{
			if (obj->WhatAmI() != AbstractType::Unit)
				continue;

			auto* const unit = static_cast<UnitClass*>(obj);
			if (unit->Owner != pOwner || unit->IsTether)
				continue;

			const int h = unit->GetHeight();
			if (h < 0 || h > Unsorted::CellHeight)
				continue;

			if (auto* const team = unit->Team)
				team->LiberateMember(unit);

			pThis->SendCommand(RadioCommand::RequestLink, unit);
			pThis->QueueMission(Mission::Unload, false);
			return; // one-after-another
		}

		// continue towards bottom-right per your original: move East once
		if (step == 0)
			cell = cell->GetNeighbourCell(FacingType::East);
	}

	// no stuck unit found → nothing to do
}

// Get all cells covered by the building, optionally including those covered by OccupyHeight.
const std::vector<CellStruct> BuildingExt::GetFoundationCells(BuildingClass* const pThis, CellStruct const baseCoords, bool includeOccupyHeight)
{
	const CellStruct foundationEnd = { 0x7FFF, 0x7FFF };
	auto const pFoundation = pThis->GetFoundationData(false);

	int occupyHeight = includeOccupyHeight ? pThis->Type->OccupyHeight : 1;

	if (occupyHeight <= 0)
		occupyHeight = 1;

	auto pCellIterator = pFoundation;

	while (*pCellIterator != foundationEnd)
		++pCellIterator;

	std::vector<CellStruct> foundationCells;
	foundationCells.reserve(static_cast<int>(std::distance(pFoundation, pCellIterator + 1)) * occupyHeight);
	pCellIterator = pFoundation;

	while (*pCellIterator != foundationEnd)
	{
		auto actualCell = baseCoords + *pCellIterator;

		for (auto i = occupyHeight; i > 0; --i)
		{
			foundationCells.emplace_back(actualCell);
			--actualCell.X;
			--actualCell.Y;
		}
		++pCellIterator;
	}

	std::sort(foundationCells.begin(), foundationCells.end(),
		[](const CellStruct& lhs, const CellStruct& rhs) -> bool
	{
		return lhs.X > rhs.X || lhs.X == rhs.X && lhs.Y > rhs.Y;
	});

	auto const it = std::unique(foundationCells.begin(), foundationCells.end());
	foundationCells.erase(it, foundationCells.end());

	return foundationCells;
}

// =============================
// load / save

template <typename T>
void BuildingExt::ExtData::Serialize(T& Stm)
{
	Stm
		.Process(this->TypeExtData)
		.Process(this->TechnoExtData)
		.Process(this->DeployedTechno)
		.Process(this->IsCreatedFromMapFile)
		.Process(this->LimboID)
		.Process(this->GrindingWeapon_LastFiredFrame)
		.Process(this->GrindingWeapon_AccumulatedCredits)
		.Process(this->CurrentAirFactory)
		.Process(this->AccumulatedIncome)
		.Process(this->CurrentLaserWeaponIndex)
		.Process(this->PoweredUpToLevel)
		.Process(this->EMPulseSW)
		;
}

void BuildingExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<BuildingClass>::LoadFromStream(Stm);
	this->Serialize(Stm);
}

void BuildingExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Extension<BuildingClass>::SaveToStream(Stm);
	this->Serialize(Stm);
}

bool BuildingExt::LoadGlobals(PhobosStreamReader& Stm)
{
	return Stm
		.Success();
}

bool BuildingExt::SaveGlobals(PhobosStreamWriter& Stm)
{
	return Stm
		.Success();
}

// =============================
// container

BuildingExt::ExtContainer::ExtContainer() : Container("BuildingClass") { }

BuildingExt::ExtContainer::~ExtContainer() = default;

// =============================
// container hooks

DEFINE_HOOK(0x43BCBD, BuildingClass_CTOR, 0x6)
{
	GET(BuildingClass*, pItem, ESI);

	auto const pExt = BuildingExt::ExtMap.TryAllocate(pItem);

	if (pExt)
	{
		pExt->TypeExtData = BuildingTypeExt::ExtMap.Find(pItem->Type);
		pExt->TechnoExtData = TechnoExt::ExtMap.Find(pItem);
	}

	return 0;
}

DEFINE_HOOK(0x43C022, BuildingClass_DTOR, 0x6)
{
	GET(BuildingClass*, pItem, ESI);

	BuildingExt::ExtMap.Remove(pItem);

	return 0;
}

DEFINE_HOOK_AGAIN(0x454190, BuildingClass_SaveLoad_Prefix, 0x5)
DEFINE_HOOK(0x453E20, BuildingClass_SaveLoad_Prefix, 0x5)
{
	GET_STACK(BuildingClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);

	BuildingExt::ExtMap.PrepareStream(pItem, pStm);

	return 0;
}

DEFINE_HOOK(0x454174, BuildingClass_Load_LightSource, 0xA)
{
	GET(BuildingClass*, pThis, EDI);

	SwizzleManagerClass::Instance.Swizzle((void**)&pThis->LightSource);

	return 0x45417E;
}

DEFINE_HOOK(0x45417E, BuildingClass_Load_Suffix, 0x5)
{
	BuildingExt::ExtMap.LoadStatic();

	return 0;
}

DEFINE_HOOK(0x454244, BuildingClass_Save_Suffix, 0x7)
{
	BuildingExt::ExtMap.SaveStatic();

	return 0;
}

// Removes setting otherwise unused field (0x6FC) in BuildingClass when building has airstrike applied on it so that it can safely be used to store BuildingExt pointer.
DEFINE_JUMP(LJMP, 0x41D9FB, 0x41DA05);


void __fastcall BuildingClass_InfiltratedBy_Wrapper(BuildingClass* pThis, void*, HouseClass* pInfiltratorHouse)
{
	const int oldBalance = pThis->Owner->Available_Money();
	// explicitly call because Ares rewrote it
	reinterpret_cast<void(__thiscall*)(BuildingClass*, HouseClass*)>(0x4571E0)(pThis, pInfiltratorHouse);

	BuildingExt::ExtMap.Find(pThis)->HandleInfiltrate(pInfiltratorHouse, oldBalance);
}

DEFINE_FUNCTION_JUMP(CALL, 0x51A00B, BuildingClass_InfiltratedBy_Wrapper);
