#pragma once
#include <Utilities/TemplateDef.h>
#include <string>

class TechnoTypeClass;

class GiftBoxData
{
public:
	Valueable<bool> Enable { false };
	// Keep these for serialization compatibility but never use them
	ValueableVector<TechnoTypeClass*> Gifts { };  // Serialized but ignored
	std::vector<int> Nums { };  // Serialized but ignored
	ValueableVector<int> RandomWeights { };
	ValueableVector<double> Chances { };
	Valueable<bool> UseChancesAndWeight { false };
	Valueable<bool> Remove { true };
	Valueable<bool> Destroy { false };
	Valueable<int> Delay { 0 };
	Valueable<int> DelayMin { 0 };
	Valueable<int> DelayMax { 0 };
	Valueable<int> RandomRange { 0 };
	Valueable<bool> EmptyCell { false };
	Valueable<bool> RandomType { false };
	Valueable<bool> OpenWhenDestoryed { false };
	Nullable<PartialVector2D<int>> RandomDelay { };
	Nullable<double> OpenWhenHealthPercent { };
	Valueable<bool> CheckPathfind { false };

	void Read(INI_EX& parser, const char* pSection);
	void GetGiftsFromINI(const char* pSection, std::vector<TechnoTypeClass*>& outGifts, std::vector<int>& outNums);

	template <typename T>
	void Serialize(T& Stm)
	{
		// Serialize everything for compatibility, but ignore Gifts/Nums at runtime
		Stm
			.Process(Enable)
			.Process(Gifts)  // Serialized for compatibility but never used
			.Process(Nums)   // Serialized for compatibility but never used
			.Process(RandomWeights)
			.Process(Chances)
			.Process(UseChancesAndWeight)
			.Process(Remove)
			.Process(Destroy)
			.Process(Delay)
			.Process(DelayMin)
			.Process(DelayMax)
			.Process(RandomRange)
			.Process(EmptyCell)
			.Process(RandomType)
			.Process(OpenWhenDestoryed)
			.Process(RandomDelay)
			.Process(OpenWhenHealthPercent)
			.Process(CheckPathfind)
			;
	}
};
