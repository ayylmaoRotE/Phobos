#include "GiftBoxData.h"
#include <Utilities/Debug.h>
#include <Utilities/Parser.h>
#include <Phobos.h>
#include <RulesClass.h>

void GiftBoxData::Read(INI_EX& parser, const char* pSection)
{
	// Check if GiftBox.Types exists to set Enable flag
	if (parser.ReadString(pSection, "GiftBox.Types"))
	{
		Enable = true;

		UseChancesAndWeight.Read(parser, pSection, "GiftBox.UseChancesAndWeight");
		RandomWeights.Read(parser, pSection, "GiftBox.RandomWeights");
		Chances.Read(parser, pSection, "GiftBox.Chances");
		Remove.Read(parser, pSection, "GiftBox.Remove");
		Destroy.Read(parser, pSection, "GiftBox.Explodes");
		Delay.Read(parser, pSection, "GiftBox.Delay");
		RandomDelay.Read(parser, pSection, "GiftBox.RandomDelay");

		if (RandomDelay.isset() && (abs(DelayMax) > 0 || abs(DelayMin) > 0))
		{
			DelayMin = abs(RandomDelay.Get().X);
			DelayMax = abs(RandomDelay.Get().Y);

			if (DelayMin > DelayMax)
				std::swap(DelayMin, DelayMax);
		}

		RandomRange.Read(parser, pSection, "GiftBox.RandomRange");
		EmptyCell.Read(parser, pSection, "GiftBox.RandomToEmptyCell");
		RandomType.Read(parser, pSection, "GiftBox.RandomType");
		OpenWhenDestoryed.Read(parser, pSection, "GiftBox.OpenWhenDestroyed");
		OpenWhenHealthPercent.Read(parser, pSection, "GiftBox.OpenWhenHealthPercent");
		CheckPathfind.Read(parser, pSection, "GiftBox.ConsiderPathFinding");
	}
}

void GiftBoxData::GetGiftsFromINI(const char* pSection, std::vector<TechnoTypeClass*>& outGifts, std::vector<int>& outNums)
{
	Debug::Log("GiftBox: GetGiftsFromINI called for section %s\n", pSection);
	
	outGifts.clear();
	outNums.clear();
	
	CCINIClass* const pINI = CCINIClass::INI_Rules;
	if (pINI) {
		INI_EX parser(pINI);
		
		// Read Gifts directly into output vector
		ValueableVector<TechnoTypeClass*> tempGifts;
		tempGifts.Read(parser, pSection, "GiftBox.Types");
		
		if (!tempGifts.empty()) {
			// Copy to output
			outGifts.assign(tempGifts.begin(), tempGifts.end());
			outNums.resize(outGifts.size(), 1);
			
			// Read Nums if they exist
			if (parser.ReadString(pSection, "GiftBox.Nums")) {
				size_t nCount = 0;
				char* context = nullptr;
				for (char* cur = strtok_s(parser.value(), Phobos::readDelims, &context);
					cur;
					cur = strtok_s(nullptr, Phobos::readDelims, &context))
				{
					int buffer = 1;
					if (Parser<int>::TryParse(cur, &buffer))
						outNums[nCount] = buffer;

					if (++nCount >= outNums.size())
						break;
				}
			}
			
			Debug::Log("GiftBox: Loaded %zu gifts from INI for section %s\n", outGifts.size(), pSection);
		} else {
			Debug::Log("GiftBox: No gifts found in INI for section %s\n", pSection);
		}
	}
}



