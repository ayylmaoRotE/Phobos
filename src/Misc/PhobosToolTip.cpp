#include <Helpers/Macro.h>
#include "PhobosToolTip.h"

#include <AircraftClass.h>
#include <BuildingClass.h>
#include <UnitClass.h>
#include <InfantryClass.h>

#include <GameOptionsClass.h>
#include <CCToolTip.h>
#include <BitFont.h>
#include <BitText.h>
#include <FPSCounter.h>
#include <Phobos.h>

#include <Ext/Side/Body.h>
#include <Ext/Surface/Body.h>
#include <Ext/House/Body.h>
#include <Ext/Sidebar/SWSidebar/SWSidebarClass.h>

#include <cwchar>
#include <cstdlib>
#include <algorithm>

PhobosToolTip PhobosToolTip::Instance;

// ---------- tiny fixed-buffer helpers (no allocations) ----------

inline void PhobosToolTip::ClearBuffer()
{
	this->Len = 0;
	this->TextBuffer[0] = L'\0';
}

inline void PhobosToolTip::Append(const wchar_t* s)
{
	if (!s) return;
	const std::size_t cap = BufferCap - 1;
	const std::size_t slen = std::wcslen(s);
	const std::size_t toCopy = (slen < (cap - Len)) ? slen : (cap - Len);
	if (toCopy)
	{
		std::wmemcpy(this->TextBuffer + Len, s, toCopy);
		this->Len += toCopy;
		this->TextBuffer[Len] = L'\0';
	}
}

inline void PhobosToolTip::AppendInt(int v)
{
	wchar_t buf[32];
	const int n = swprintf_s(buf, L"%d", v);
	if (n > 0) Append(buf);
}

inline void PhobosToolTip::AppendFixed2(int mm, int ss)
{
	wchar_t buf[8];
	const int n = swprintf_s(buf, L"%02d:%02d", mm, ss);
	if (n > 0) Append(buf);
}

// ---------- feature toggles ----------

inline bool PhobosToolTip::IsEnabled() const
{
	return Phobos::UI::ExtendedToolTips;
}

inline const wchar_t* PhobosToolTip::GetUIDescription(TechnoTypeExt::ExtData* pData) const
{
	return Phobos::Config::ToolTipDescriptions && !pData->UIDescription.Get().empty()
		? pData->UIDescription.Get().Text
		: nullptr;
}

inline const wchar_t* PhobosToolTip::GetUIDescription(SWTypeExt::ExtData* pData) const
{
	return Phobos::Config::ToolTipDescriptions && !pData->UIDescription.Get().empty()
		? pData->UIDescription.Get().Text
		: nullptr;
}

// ---------- compute helpers ----------

inline int PhobosToolTip::GetBuildTime(TechnoTypeClass* pType) const
{
	// Preserve original trick to access TimeToBuild() for each final class.
	static char pTrick[0x6C8]; // big enough to hold all derived classes
	switch (pType->WhatAmI())
	{
	case AbstractType::BuildingType:
		VTable::Set(pTrick, BuildingClass::AbsVTable);
		reinterpret_cast<BuildingClass*>(pTrick)->Type = (BuildingTypeClass*)pType;
		break;
	case AbstractType::AircraftType:
		VTable::Set(pTrick, AircraftClass::AbsVTable);
		reinterpret_cast<AircraftClass*>(pTrick)->Type = (AircraftTypeClass*)pType;
		break;
	case AbstractType::InfantryType:
		VTable::Set(pTrick, InfantryClass::AbsVTable);
		reinterpret_cast<InfantryClass*>(pTrick)->Type = (InfantryTypeClass*)pType;
		break;
	case AbstractType::UnitType:
		VTable::Set(pTrick, UnitClass::AbsVTable);
		reinterpret_cast<UnitClass*>(pTrick)->Type = (UnitTypeClass*)pType;
		break;
	}

	reinterpret_cast<TechnoClass*>(pTrick)->Owner = HouseClass::CurrentPlayer;
	const int nTimeToBuild = reinterpret_cast<TechnoClass*>(pTrick)->TimeToBuild();
	return std::max(54, nTimeToBuild); // >=54 frames as before
}

inline int PhobosToolTip::GetPower(TechnoTypeClass* pType) const
{
	switch (pType->WhatAmI())
	{
	case AbstractType::AircraftType:
	case AbstractType::InfantryType:
	case AbstractType::UnitType:
	{
		if (!Phobos::Config::UnitPowerDrain) { return 0; }
		const auto pExt = TechnoTypeExt::ExtMap.Find(pType);
		return pExt->Power;
	}
	case AbstractType::BuildingType:
	{
		const auto pBldType = (BuildingTypeClass*)pType;
		return pBldType->PowerBonus - pBldType->PowerDrain;
	}
	default:
		return 0;
	}
}

// Match original RealTimeTimers semantics.
static inline int TickTimeToSeconds(int tickTime)
{
	if (!Phobos::Config::RealTimeTimers)
	{
		return tickTime / 15;
	}

	if (Phobos::Config::RealTimeTimers_Adaptive
		|| GameOptionsClass::Instance.GameSpeed == 0
		|| (Phobos::Misc::CustomGS && !SessionClass::IsMultiplayer()))
	{
		const int fps = std::max((int)FPSCounter::CurrentFrameRate, 1);
		return tickTime / fps;
	}

	return tickTime / (60 / GameOptionsClass::Instance.GameSpeed);
}

// ---------- public API ----------
// NOTE: GetBuffer() is inline in the header; there is NO definition here.

void PhobosToolTip::HelpText(BuildType& cameo)
{
	if (cameo.ItemType == AbstractType::Special)
		this->HelpText_Super(cameo.ItemIndex);
	else
		this->HelpText_Techno(ObjectTypeClass::GetTechnoType(cameo.ItemType, cameo.ItemIndex));
}

void PhobosToolTip::HelpText_Techno(TechnoTypeClass* pType)
{
	if (!pType) { return; }

	auto const pData = TechnoTypeExt::ExtMap.Find(pType);

	const int nBuildTime = TickTimeToSeconds(this->GetBuildTime(pType));
	const int nSec = nBuildTime % 60;
	const int nMin = nBuildTime / 60;
	const int cost = pType->GetActualCost(HouseClass::CurrentPlayer);

	ClearBuffer();
	Append(pType->UIName); Append(L"\n");
	if (cost < 0) { Append(L"+"); } // keep original semantic for refunds
	Append(Phobos::UI::CostLabel); AppendInt(std::abs(cost)); Append(L" ");
	Append(Phobos::UI::TimeLabel); AppendFixed2(nMin, nSec);

	if (auto const nPower = this->GetPower(pType))
	{
		Append(L" "); Append(Phobos::UI::PowerLabel);
		if (nPower > 0) { Append(L"+"); }
		AppendInt(nPower);
	}

	if (auto const pDesc = this->GetUIDescription(pData))
	{
		Append(L"\n"); Append(pDesc);
	}
}

void PhobosToolTip::HelpText_Super(int swidx)
{
	auto const pSuper = HouseClass::CurrentPlayer->Supers.Items[swidx];
	auto const pType = pSuper->Type;
	auto const pData = SWTypeExt::ExtMap.Find(pType);

	const int rechargeTime = TickTimeToSeconds(pSuper->GetRechargeTime());
	const int sec = rechargeTime % 60;
	const int min = rechargeTime / 60;

	ClearBuffer();
	Append(pType->UIName);

	bool showSth = false;

	if (const int nCost = std::abs(pData->Money_Amount))
	{
		Append(L"\n");
		if (pData->Money_Amount > 0) { Append(L"+"); }
		Append(Phobos::UI::CostLabel); AppendInt(nCost);
		showSth = true;
	}

	if (int nPoints = std::abs(pData->BattlePoints_Amount))
	{
		Append(L"\n");
		Append(Phobos::UI::BattlePoints_Label);
		Append(pData->BattlePoints_Amount > 0 ? L"+" : L"-");
		AppendInt(nPoints);
		showSth = true;
	}

	if (int nPoints = std::abs(pData->CommanderPoints_Amount))
	{
		Append(L"\n");
		Append(Phobos::UI::CommanderPoints_Label);
		Append(pData->CommanderPoints_Amount > 0 ? L"+" : L"-");
		AppendInt(nPoints);
		showSth = true;
	}

	if (rechargeTime > 0)
	{
		if (!showSth) { Append(L"\n"); }
		if (showSth) { Append(L" "); }
		Append(Phobos::UI::TimeLabel); AppendFixed2(min, sec);
		showSth = true;
	}

	auto const& sw_ext = HouseExt::ExtMap.Find(HouseClass::CurrentPlayer)->SuperExts[swidx];
	const int sw_shots = pData->SW_Shots;
	const int remain_shots = pData->SW_Shots - sw_ext.ShotCount;
	if (sw_shots > 0)
	{
		if (!showSth) { Append(L"\n"); }
		wchar_t buffer[64];
		swprintf_s(buffer, Phobos::UI::SWShotsFormat, remain_shots, sw_shots);
		if (showSth) { Append(L" "); }
		Append(buffer);
	}

	if (auto const pDesc = this->GetUIDescription(pData))
	{
		Append(L"\n"); Append(pDesc);
	}
}

// ---------------- hooks (setter-style register API) ----------------

DEFINE_HOOK(0x6A9316, SidebarClass_StripClass_HelpText, 0x6)
{
	PhobosToolTip::Instance.IsCameo = true;

	if (!PhobosToolTip::Instance.IsEnabled())

		return 0;

	GET(StripClass*, pThis, EAX);
	PhobosToolTip::Instance.HelpText(pThis->Cameos[0]);
	R->EAX(reinterpret_cast<DWORD>(L"X"));
	return 0x6A93DE;
}

DEFINE_HOOK(0x4AE51E, DisplayClass_GetToolTip_HelpText, 0x6)
{
	enum { ApplyToolTip = 0x4AE69D };

	if (SWSidebarClass::IsEnabled())
	{
		const auto& swSidebar = SWSidebarClass::Instance;

		if (const auto button = swSidebar.CurrentButton)
		{
			PhobosToolTip::Instance.IsCameo = true;

			if (PhobosToolTip::Instance.IsEnabled())
			{
				PhobosToolTip::Instance.HelpText_Super(button->SuperIndex);
				R->EAX(reinterpret_cast<DWORD>(PhobosToolTip::Instance.GetBuffer()));
			}
			else
			{
				const auto pSuper = HouseClass::CurrentPlayer->Supers[button->SuperIndex];
				R->EAX(reinterpret_cast<DWORD>(pSuper->Type->UIName));
			}

			return ApplyToolTip;
		}
		else if (swSidebar.CurrentColumn || (swSidebar.ToggleButton && swSidebar.ToggleButton->IsHovering))
		{
			R->EAX(0);
			return ApplyToolTip;
		}
	}

	return 0;
}

DEFINE_HOOK(0x478EE1, CCToolTip_Draw2_SetBuffer, 0x6)
{
	if (PhobosToolTip::Instance.IsEnabled() && PhobosToolTip::Instance.IsCameo)
		R->EDI(reinterpret_cast<DWORD>(PhobosToolTip::Instance.GetBuffer()));
	return 0;
}

DEFINE_HOOK(0x478E10, CCToolTip_Draw1, 0x0)
{
	GET(CCToolTip*, pThis, ECX);
	GET_STACK(const bool, bFullRedraw, 0x4);

	if (!bFullRedraw || PhobosToolTip::Instance.IsCameo)
	{
		PhobosToolTip::Instance.IsCameo = false;
		PhobosToolTip::Instance.SlaveDraw = false;
		pThis->ToolTipManager::Process();
	}

	if (pThis->CurrentToolTip)
	{
		if (!bFullRedraw)
			PhobosToolTip::Instance.SlaveDraw = PhobosToolTip::Instance.IsCameo;

		pThis->FullRedraw = bFullRedraw;
		pThis->DrawText(pThis->CurrentToolTipData);
	}
	return 0x478E25;
}

DEFINE_HOOK(0x478E4A, CCToolTip_Draw2_SetSurface, 0x6)
{
	if (PhobosToolTip::Instance.SlaveDraw)
	{
		R->ESI(reinterpret_cast<DWORD>(DSurface::Composite));
		return 0x478ED3;
	}
	return 0;
}

DEFINE_HOOK(0x478EF8, CCToolTip_Draw2_SetMaxWidth, 0x5)
{
	if (PhobosToolTip::Instance.IsCameo)
	{
		if (Phobos::UI::MaxToolTipWidth > 0)
			R->EAX(Phobos::UI::MaxToolTipWidth);
		else
			R->EAX(DSurface::ViewBounds.Width);
	}
	return 0;
}

DEFINE_HOOK(0x478F52, CCToolTip_Draw2_SetX, 0x8)
{
	if (PhobosToolTip::Instance.SlaveDraw)
		R->EAX(R->EAX() + DSurface::Sidebar->GetWidth());

	return 0;
}

DEFINE_HOOK(0x478F77, CCToolTip_Draw2_SetY, 0x6)
{
	if (PhobosToolTip::Instance.IsCameo)
	{
		LEA_STACK(RectangleStruct*, Rect, STACK_OFFSET(0x3C, -0x20));

		int const maxHeight = DSurface::ViewBounds.Height - 32;

		if (Rect->Height > maxHeight)
			Rect->Y += maxHeight - Rect->Height;

		if (Rect->Y < 0)
			Rect->Y = 0;
	}
	return 0;
}

DEFINE_HOOK(0x479029, CCToolTip_Draw2_SetPadding, 0x5)
{
	if (PhobosToolTip::Instance.IsCameo)
	{
		if (Phobos::UI::MaxToolTipWidth > 0)
			R->EDX(R->EDX() - 5);
	}

	return 0;
}

void __declspec(naked) _CCToolTip_Draw2_FillRect_RET()
{
	ADD_ESP(8);
	JMP(0x478FE1);
}

DEFINE_HOOK(0x478FDC, CCToolTip_Draw2_FillRect, 0x5)
{
	GET(SurfaceExt*, pThis, ESI);
	LEA_STACK(RectangleStruct*, pRect, STACK_OFFSET(0x44, -0x10));

	const bool isCameo = PhobosToolTip::Instance.IsCameo;

	if (isCameo && Phobos::UI::AnchoredToolTips
		&& PhobosToolTip::Instance.IsEnabled()
		&& Phobos::Config::ToolTipDescriptions)
	{
		LEA_STACK(LTRBStruct*, pTextRect, STACK_OFFSET(0x44, -0x20));

		if (const auto pButton = SWSidebarClass::Instance.CurrentButton)
		{
			GET_STACK(const int, textHeight, STACK_OFFSET(0x44, -0x28));

			const auto pColumn = SWSidebarClass::Instance.Columns[pButton->ColumnIndex];
			const int x = pColumn->X + pColumn->Width + 2;
			const int y = std::clamp(pButton->Y + 3, 0, DSurface::ViewBounds.Height - textHeight);
			pRect->X = x;
			pTextRect->Right += (x - pTextRect->Left);
			pTextRect->Left = x;
			pRect->Y = y;
			pTextRect->Bottom += (y - pTextRect->Top);
			pTextRect->Top = y;
		}
		else
		{
			const int x = DSurface::SidebarBounds.X - pRect->Width - 2;
			pRect->X = x;
			pTextRect->Left = x;
		}
	}
	return 0;
}

