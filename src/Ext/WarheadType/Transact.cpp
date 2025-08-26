#include "Body.h"

#include <Utilities/Enum.h>
#include <Utilities/GeneralUtils.h>
#include <Ext/Techno/Body.h>
#include <Helpers/Macro.h>

#include <algorithm>
#include <cmath>

static __forceinline double safe_div(double num, double den)
{
	// behavior-preserving guard: if den==0, old code would have UB; we just yield 0 delta
	return (den != 0.0) ? (num / den) : 0.0;
}

int AddExpCustom(VeterancyStruct* vstruct, int targetCost, int exp)
{
	// Early-out when nothing to do
	if (exp == 0)
	{
		return 0;
	}

	const double vr = RulesClass::Instance->VeteranRatio;
	const double den = static_cast<double>(targetCost) * vr;

	// Keep math order to preserve rounding behavior
	const double toBeAdded = safe_div(static_cast<double>(exp), den);

	// "Transferred" uses current veterancy and the same denominator; preserve abs and cast order
	const double absAdded = std::abs(toBeAdded);
	const double capped = std::min(static_cast<double>(vstruct->Veterancy), static_cast<double>(absAdded));
	int transferred = static_cast<int>(capped * den);

	if (exp < 0 && transferred <= 0)
	{
		// negative grant but nothing to remove: reset like original
		vstruct->Reset();
		transferred = 0;
	}
	else
	{
		vstruct->Add(toBeAdded);
	}

	// Clamp at Elite (2.0)
	if (vstruct->IsElite())
	{
		vstruct->SetElite();
	}
	return transferred;
}

int WarheadTypeExt::ExtData::TransactOneValue(TechnoClass* pTechno, TechnoTypeClass* pTechnoType, int transactValue, TransactValueType valueType)
{
	if (!pTechno)
	{
		return 0;
	}

	switch (valueType)
	{
	case TransactValueType::Experience:
	{
		const int cost = pTechnoType ? pTechnoType->GetActualCost(pTechno->Owner) : 0;
		return AddExpCustom(&pTechno->Veterancy, cost, transactValue);
	}
	default:
		break;
	}
	return 0;
}

int WarheadTypeExt::ExtData::TransactGetValue(TechnoClass* pTarget, TechnoClass* pOwner, int flat, double percent, bool calcFromTarget)
{
	// Flat component
	const int flatValue = flat;

	// Percent component (same semantics; avoid redundant CLOSE_ENOUGH calls)
	int percentValue = 0;
	if (!CLOSE_ENOUGH(percent, 0.0))
	{
		if (calcFromTarget)
		{
			percentValue = pTarget ? static_cast<int>(pTarget->GetTechnoType()->GetActualCost(pTarget->Owner) * percent) : 0;
		}
		else
		{
			percentValue = pOwner ? static_cast<int>(pOwner->GetTechnoType()->GetActualCost(pOwner->Owner) * percent) : 0;
		}
	}

	// Choose larger magnitude like original
	return (std::abs(percentValue) > std::abs(flatValue)) ? percentValue : flatValue;
}


TransactData WarheadTypeExt::ExtData::TransactGetSourceAndTarget(TechnoClass* pTarget, TechnoTypeClass* pTargetType, TechnoClass* pOwner, TechnoTypeClass* pOwnerType, int targets)
{
	TransactData allVal;
	std::vector<int> sourceValues;
	std::vector<int> targetValues;

	const auto IsTargetAffected = [this](TechnoClass* pThis, TechnoClass* pTarget , bool DisablepThisCheck ,bool DisablepTargetCheck , bool IsFlipped = false)
	{
		if (!pThis)
			return DisablepThisCheck;

		if (!CanDealDamage(pTarget, true))
			return IsFlipped;

		if (!pThis->GetOwningHouse())
			return true;

		if (!CanTargetHouse(pThis->GetOwningHouse(), pTarget))
			return false;

		if (!pTarget)
			return DisablepTargetCheck;

		if (!pTarget->GetTechnoType()->Trainable && this->Transact_Experience_IgnoreNotTrainable)
			return false;

		return true;
	};

	// SOURCE
	//		Experience
	int sourceExp = IsTargetAffected(pOwner, pTarget , !this->Transact_Experience_Target_Percent_CalcFromSource, !this->Transact_Experience_Source_Percent_CalcFromTarget) ?
		TransactGetValue(pTarget, pOwner,
		this->Transact_Experience_Source_Flat,
		this->Transact_Experience_Source_Percent,
		this->Transact_Experience_Source_Percent_CalcFromTarget) : 0;

	sourceValues.push_back(sourceExp / targets);
	// TARGET
	//		Experience
	int targetExp = IsTargetAffected(pTarget, pOwner , !this->Transact_Experience_Source_Percent_CalcFromTarget , !this->Transact_Experience_Target_Percent_CalcFromSource , true) ?
		TransactGetValue(pOwner, pTarget,
		this->Transact_Experience_Target_Flat, this->Transact_Experience_Target_Percent,
		this->Transact_Experience_Target_Percent_CalcFromSource) : 0;

	targetValues.push_back(targetExp / targets);

	allVal.emplace_back(sourceValues, targetValues, TransactValueType::Experience) ;

	return allVal;
}

void WarheadTypeExt::ExtData::TransactOnOneUnit(TechnoClass* pTarget, TechnoClass* pOwner, int targets)
{
	auto const pTargetType = pTarget ? pTarget->GetTechnoType() : nullptr;
	auto const pOwnerType = pOwner ? pOwner->GetTechnoType() : nullptr;

	TransactData allValues = this->TransactGetSourceAndTarget(pTarget, pTargetType, pOwner, pOwnerType, targets);

	for (const auto& [vecsourceValue, vectargetValue, nTransactType] : allValues)
	{
		for (unsigned int i = 0; i < vecsourceValue.size(); i++)
		{
			const int sourceValue = vecsourceValue[i];
			const int targetValue = vectargetValue[i];

			// Transact (A loses B gains)
			if (sourceValue != 0 && targetValue != 0 && targetValue * sourceValue < 0)
			{
				int transactValue = std::abs(sourceValue) > std::abs(targetValue) ? std::abs(targetValue) : std::abs(sourceValue);

				if (sourceValue < 0)
				{
					transactValue = TransactOneValue(pOwner, pOwnerType, -transactValue, nTransactType);
					TransactOneValue(pTarget, pTargetType, transactValue, nTransactType);
				}
				else
				{
					transactValue = TransactOneValue(pTarget, pTargetType, -transactValue, nTransactType);
					TransactOneValue(pOwner, pOwnerType, transactValue, nTransactType);
				}

				return;
			}
			// Out-of-thin-air grants
			if (sourceValue != 0)
			{
				TransactOneValue(pOwner, pOwnerType, sourceValue, nTransactType);
			}

			if (targetValue != 0)
			{
				TransactOneValue(pTarget, pTargetType, targetValue, nTransactType);
			}
		}
	}
}

void WarheadTypeExt::ExtData::TransactOnAllUnits(std::vector<TechnoClass*>& nVec, HouseClass* pHouse,TechnoClass* pOwner)
{
	//since we are on last chain of the event , we can do these thing
	const auto NotEligible = [this, pHouse , pOwner](TechnoClass* const pTech)
	{
		if (!pTech || pTech->InLimbo || !pTech->IsAlive)
			return true;

		if (!pTech->GetTechnoType()->Trainable && this->Transact_Experience_IgnoreNotTrainable)
			return true;

		return !CanTargetHouse(pHouse, pTech);
	};

	// Need to include fast_remove_if utility
	nVec.erase(std::remove_if(nVec.begin(), nVec.end(), NotEligible), nVec.end());

	if (!nVec.empty()) {

		const int count = !this->Transact_SpreadAmongTargets ? 1: nVec.size();

		std::for_each(nVec.begin(), nVec.end(), [this, pOwner, pHouse ,&count](TechnoClass* const pTech) {
			TransactOnOneUnit(pTech, pOwner, count);
		});

	} else {
		TransactOnOneUnit(nullptr, pOwner, 1);
	}

}
