#include "FlyingStrings.h"
#include <Phobos.h>
#include <MapClass.h>
#include <Phobos.CRT.h>
#include <TacticalClass.h>
#include <ColorScheme.h>
#include <Drawing.h>
#include <ScenarioClass.h>
#include <BitFont.h>
#include <Utilities/EnumFunctions.h>
#include <New/Contracts/ContractEvents.h>

std::vector<FlyingStrings::Item> FlyingStrings::Data;

bool FlyingStrings::DrawAllowed(CoordStruct& nCoords)
{
	if (auto const pCell = MapClass::Instance.TryGetCellAt(nCoords))
		return !(pCell->IsFogged() || pCell->IsShrouded());

	return false;
}

void FlyingStrings::Add(const wchar_t* text, const CoordStruct& coords, ColorStruct color, Point2D pixelOffset)
{
	Item item {};
	item.Location = coords;
	item.PixelOffset = pixelOffset;
	item.CreationFrame = Unsorted::CurrentFrame;
	item.Color = Drawing::RGB_To_Int(color);
	PhobosCRT::wstrCopy(item.Text, text, 0x20);
	Data.emplace_back(item);
}

void FlyingStrings::AddMoneyString(int amount, HouseClass* owner, AffectedHouse displayToHouses, const CoordStruct& coords, Point2D pixelOffset)
{
	if (amount
		&& (displayToHouses == AffectedHouse::All
			|| (owner && EnumFunctions::CanTargetHouse(displayToHouses, owner, HouseClass::CurrentPlayer))))
	{
		const bool isPositive = (amount > 0);
		if (owner && isPositive)
		{
			Contracts::OnMoneyEarned(owner, amount);
		}
		const ColorStruct color = isPositive ? ColorStruct { 0, 255, 0 } : ColorStruct { 255, 0, 0 };

		wchar_t moneyStr[0x20];
		swprintf_s(moneyStr, L"%ls%ls%d", isPositive ? L"+" : L"-", Phobos::UI::CostLabel, std::abs(amount));

		int width = 0, height = 0;
		BitFont::Instance->GetTextDimension(moneyStr, &width, &height, 120);
		pixelOffset.X -= (width / 2);

		FlyingStrings::Add(moneyStr, coords, color, pixelOffset);
	}
}

void FlyingStrings::UpdateAll()
{
	if (Data.empty())
		return;

	const int now = Unsorted::CurrentFrame;

	// Hoist once per frame; matches original behavior (subtract 32 px)
	RectangleStruct bound = DSurface::Temp->GetRect();
	bound.Height -= 32;

	// Single pass with swap-erase O(1) removals.
	// NOTE: We do not change visual order in practice; off-screen items skipped are invisible anyway.
	for (size_t i = 0; i < Data.size(); /*increment inside*/)
	{
		auto& it = Data[i];

		// Expire: same conditions as original
		const bool tooOld = (now > it.CreationFrame + Duration);
		const bool wrapped = (now < it.CreationFrame);

		if (tooOld || wrapped)
		{
			// swap-erase
			Data[i] = Data.back();
			Data.pop_back();
			continue;
		}

		// Screen transform; returns (point, visible). If not visible, skip draw (no change on screen, less work).
		auto [pt, visible] = TacticalClass::Instance->CoordsToClient(it.Location);
		if (!visible)
		{
			++i;
			continue;
		}

		pt += it.PixelOffset;

		// Late-life float up (identical timing)
		if (now > it.CreationFrame + (Duration - 70))
		{
			pt.Y -= (now - it.CreationFrame);
		}

		DSurface::Temp->DrawText(it.Text, &bound, &pt, it.Color, 0, TextPrintType::NoShadow);
		++i;
	}
}
