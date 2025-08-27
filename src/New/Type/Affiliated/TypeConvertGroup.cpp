#include <Ext/Techno/Body.h>
#include <AnimClass.h>
#include <ScenarioClass.h>
#include "TypeConvertGroup.h"

void TypeConvertGroup::Convert(FootClass* pTargetFoot, const std::vector<TypeConvertGroup>& convertPairs, HouseClass* pOwner, AnimTypeClass* pConvertAnim)
{
	for (const auto& [fromTypes, toType, affectedHouses, chance] : convertPairs)
	{
		if (!toType.Get())
			continue;

		if (pOwner && !EnumFunctions::CanTargetHouse(affectedHouses, pOwner, pTargetFoot->Owner))
			continue;

		// Check conversion chance using deterministic game RNG
		if (ScenarioClass::Instance->Random.RandomDouble() > chance.Get())
			continue;

		if (fromTypes.size())
		{
			for (const auto& from : fromTypes)
			{
				// Check if the target matches upgrade-from TechnoType and it has something to upgrade to
				if (from == pTargetFoot->GetTechnoType())
				{
					if (TechnoExt::ConvertToType(pTargetFoot, toType))
					{
						// Play conversion animation if specified
						if (pConvertAnim)
						{
							TechnoClass* pDrawer = pTargetFoot;
							if (pTargetFoot->InLimbo && pTargetFoot->Transporter)
								pDrawer = pTargetFoot->Transporter;
							else if (pTargetFoot->InLimbo && !pTargetFoot->Transporter)
								pDrawer = nullptr;

							if (pDrawer)
							{
								auto pAnim = GameCreate<AnimClass>(pConvertAnim, pDrawer->Location);
								pAnim->SetOwnerObject(pDrawer);
							}
						}
					}
					goto end; // Breaking out of nested loops without extra checks one of the very few remaining valid usecases for goto, leave it be.
				}
			}
		}
		else
		{
			if (TechnoExt::ConvertToType(pTargetFoot, toType))
			{
				// Play conversion animation if specified
				if (pConvertAnim)
				{
					TechnoClass* pDrawer = pTargetFoot;
					if (pTargetFoot->InLimbo && pTargetFoot->Transporter)
						pDrawer = pTargetFoot->Transporter;
					else if (pTargetFoot->InLimbo && !pTargetFoot->Transporter)
						pDrawer = nullptr;

					if (pDrawer)
					{
						auto pAnim = GameCreate<AnimClass>(pConvertAnim, pDrawer->Location);
						pAnim->SetOwnerObject(pDrawer);
					}
				}
			}
			break;
		}
	}
end:
	return;
}


bool TypeConvertGroup::Load(PhobosStreamReader& stm, bool registerForChange)
{
	return this->Serialize(stm);
}

bool TypeConvertGroup::Save(PhobosStreamWriter& stm) const
{
	return const_cast<TypeConvertGroup*>(this)->Serialize(stm);
}

void TypeConvertGroup::Parse(std::vector<TypeConvertGroup>& list, INI_EX& exINI, const char* pSection, AffectedHouse defaultAffectHouse)
{
	for (size_t i = 0; ; ++i)
	{
		char tempBuffer[32];
		ValueableVector<TechnoTypeClass*> convertFrom;
		Nullable<TechnoTypeClass*> convertTo;
		Nullable<AffectedHouse> convertAffectedHouses;
		Valueable<double> convertChance { 1.0 };
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "Convert%d.From", i);
		convertFrom.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "Convert%d.To", i);
		convertTo.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "Convert%d.AffectedHouses", i);
		convertAffectedHouses.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "Convert%d.Chance", i);
		convertChance.Read(exINI, pSection, tempBuffer);

		if (!convertTo.isset())
			break;

		if (!convertAffectedHouses.isset())
			convertAffectedHouses = defaultAffectHouse;

		list.emplace_back(convertFrom, convertTo, convertAffectedHouses, convertChance.Get());
	}
	ValueableVector<TechnoTypeClass*> convertFrom;
	Nullable<TechnoTypeClass*> convertTo;
	Nullable<AffectedHouse> convertAffectedHouses;
	Valueable<double> convertChance { 1.0 };
	convertFrom.Read(exINI, pSection, "Convert.From");
	convertTo.Read(exINI, pSection, "Convert.To");
	convertAffectedHouses.Read(exINI, pSection, "Convert.AffectedHouses");
	convertChance.Read(exINI, pSection, "Convert.Chance");
	if (convertTo.isset())
	{
		if (!convertAffectedHouses.isset())
			convertAffectedHouses = defaultAffectHouse;

		if (list.size())
			list[0] = { convertFrom, convertTo, convertAffectedHouses, convertChance.Get() };
		else
			list.emplace_back(convertFrom, convertTo, convertAffectedHouses, convertChance.Get());
	}
}

template <typename T>
bool TypeConvertGroup::Serialize(T& stm)
{
	return stm
		.Process(this->FromTypes)
		.Process(this->ToType)
		.Process(this->AppliedTo)
		.Process(this->Chance)
		.Success();
}
