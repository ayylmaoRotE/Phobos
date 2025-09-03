#pragma once

#include <Utilities/EnumFunctions.h>

class TypeConvertGroup
{
public:
	ValueableVector<TechnoTypeClass*> FromTypes;
	Nullable<TechnoTypeClass*> ToType;
	Nullable<AffectedHouse> AppliedTo;
	Valueable<double> Chance;

	// Constructors
	TypeConvertGroup() : Chance(1.0) { }
	TypeConvertGroup(const ValueableVector<TechnoTypeClass*>& fromTypes, const Nullable<TechnoTypeClass*>& toType, const Nullable<AffectedHouse>& appliedTo, double chance = 1.0)
		: FromTypes(fromTypes), ToType(toType), AppliedTo(appliedTo), Chance(chance) { }

	bool Load(PhobosStreamReader& stm, bool registerForChange);
	bool Save(PhobosStreamWriter& stm) const;

	static void Parse(std::vector<TypeConvertGroup>& list, INI_EX& exINI, const char* section, AffectedHouse defaultAffectHouse);

	static void Convert(FootClass* pTargetFoot, const std::vector<TypeConvertGroup>& convertPairs, HouseClass* pOwner, AnimTypeClass* pConvertAnim = nullptr);

private:
	template <typename T>
	bool Serialize(T& stm);
};
