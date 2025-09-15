#include "Body.h"
#include "SWSidebar/SWSidebarClass.h"

#include <HouseClass.h>
#include <FactoryClass.h>
#include <FileSystem.h>

#include <SuperClass.h>
#include <Ext/Side/Body.h>
#include <Misc/MessageColumn.h>
#include <Ext/SWType/Body.h>
#include <Ext/House/Body.h>

DEFINE_HOOK(0x6A593E, SidebarClass_InitForHouse_AdditionalFiles, 0x5)
{
	char filename[0x20];

	for (int i = 0; i < 4; i++)
	{
		sprintf_s(filename, "tab%02dpp.shp", i);
		SidebarExt::TabProducingProgress[i] = GameCreate<SHPReference>(filename);
	}

	return 0;
}

DEFINE_HOOK(0x6A5EA1, SidebarClass_UnloadShapes_AdditionalFiles, 0x5)
{
	for (int i = 0; i < 4; i++)
	{
		if (SidebarExt::TabProducingProgress[i])
		{
			GameDelete(SidebarExt::TabProducingProgress[i]);
			SidebarExt::TabProducingProgress[i] = nullptr;
		}
	}

	return 0;
}

DEFINE_HOOK(0x6A6EB1, SidebarClass_DrawIt_ProducingProgress, 0x6)
{
	if (Phobos::UI::ProducingProgress_Show)
	{
		const auto pPlayer = HouseClass::CurrentPlayer;
		const auto pSideExt = SideExt::ExtMap.Find(SideClass::Array.GetItem(HouseClass::CurrentPlayer->SideIndex));
		const int XOffset = pSideExt->Sidebar_GDIPositions ? 29 : 32;
		const int XBase = (pSideExt->Sidebar_GDIPositions ? 26 : 20) + pSideExt->Sidebar_ProducingProgress_Offset.Get().X;
		const int YBase = 197 + pSideExt->Sidebar_ProducingProgress_Offset.Get().Y;

		for (int i = 0; i < 4; i++)
		{
			if (const auto pSHP = SidebarExt::TabProducingProgress[i])
			{
				const auto rtti = i == 0 || i == 1 ? AbstractType::BuildingType : AbstractType::InfantryType;
				FactoryClass* pFactory = nullptr;

				if (i != 3)
				{
					pFactory = pPlayer->GetPrimaryFactory(rtti, false, i == 1 ? BuildCat::Combat : BuildCat::DontCare);
				}
				else
				{
					pFactory = pPlayer->GetPrimaryFactory(AbstractType::UnitType, false, BuildCat::DontCare);
					if (!pFactory || !pFactory->Object)
						pFactory = pPlayer->GetPrimaryFactory(AbstractType::UnitType, true, BuildCat::DontCare);
					if (!pFactory || !pFactory->Object)
						pFactory = pPlayer->GetPrimaryFactory(AbstractType::AircraftType, false, BuildCat::DontCare);
				}

				const int idxFrame = pFactory
					? (int)(((double)pFactory->GetProgress() / 54) * (pSHP->Frames - 1))
					: -1;

				Point2D vPos = { XBase + i * XOffset, YBase };
				RectangleStruct sidebarRect = DSurface::Sidebar->GetRect();

				if (idxFrame != -1)
				{
					DSurface::Sidebar->DrawSHP(FileSystem::SIDEBAR_PAL, pSHP, idxFrame, &vPos,
						&sidebarRect, BlitterFlags::bf_400, 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
				}
			}
		}
	}

	return 0;
}

DEFINE_HOOK(0x72FCB5, InitSideRectangles_CenterBackground, 0x5)
{
	if (Phobos::UI::CenterPauseMenuBackground)
	{
		GET(RectangleStruct*, pRect, EAX);
		GET_STACK(const int, width, STACK_OFFSET(0x18, -0x4));
		GET_STACK(const int, height, STACK_OFFSET(0x18, -0x8));

		pRect->X = (width - 168 - pRect->Width) / 2;
		pRect->Y = (height - 32 - pRect->Height) / 2;

		R->EAX(pRect);
	}

	return 0;
}

#pragma region NewButtonsRelated

DEFINE_HOOK(0x692419, DisplayClass_ProcessClickCoords_SkipOnNewButtons, 0x7)
{
	enum { DoNothing = 0x6925FC };

	return (SWSidebarClass::IsEnabled() && SWSidebarClass::Instance.CurrentColumn
		|| SWSidebarClass::Instance.ToggleButton && SWSidebarClass::Instance.ToggleButton->IsHovering
		|| MessageColumnClass::Instance.IsBlocked())
		? DoNothing : 0;
}

DEFINE_HOOK(0x6A5082, SidebarClass_InitClear_InitializeNewButtons, 0x5)
{
	SWSidebarClass::Instance.InitClear();
	MessageColumnClass::Instance.InitClear();
	return 0;
}

DEFINE_HOOK(0x6A5839, SidebarClass_InitIO_InitializeNewButtons, 0x5)
{
	SWSidebarClass::Instance.InitIO();
	MessageColumnClass::Instance.InitIO();
	return 0;
}

DEFINE_HOOK_AGAIN(0x4E13B2, GadgetClass_DTOR_ClearCurrentOverGadget, 0x6)
DEFINE_HOOK(0x4E1A84, GadgetClass_DTOR_ClearCurrentOverGadget, 0x6)
{
	GadgetClass* const pThis = (R->Origin() == 0x4E1A84) ? R->ESI<GadgetClass*>() : R->ECX<GadgetClass*>();
	AnnounceInvalidPointer(Make_Global<GadgetClass*>(0x8B3E94), pThis);
	return 0;
}

// Vanilla sidebar superweapon cameo darkening - replicate money check for BP/CP/AuxTechnos
DEFINE_HOOK(0x6A99B7, TabCameoListClass_Draw_SuperDarken, 0x5)
{
	// idxSW is the superweapon index whose cameo is being drawn
	GET(int, idxSW, EDI);

	bool darken = false;

	// Cheap early guards + hoisted lookups
	if (auto* const pPlayer = HouseClass::CurrentPlayer)
	{
		if (auto* const pSW = pPlayer->Supers.GetItem(idxSW))
		{
			if (auto* const pSWType = pSW->Type)
			{
				if (auto* const pSWExt = SWTypeExt::ExtMap.Find(pSWType))
				{

					// Only apply extra darken rules when the SW is otherwise "ready".
					// Base engine already shades when not ready.
					if (pSW->IsReady)
					{

						// Requirement 1: Money (Ares original)
						if (!pSW->Owner->CanTransactMoney(pSWExt->Money_Amount))
						{
							darken = true;
						}

						// Requirement 2/3: Battle / Commander Points
						if (auto* const pOwnerExt = HouseExt::ExtMap.Find(pSW->Owner))
						{
							if (!darken && pSWExt->BattlePoints_Amount != 0 &&
								!pOwnerExt->CanTransactBattlePoints(pSWExt->BattlePoints_Amount))
							{
								darken = true;
							}
							if (!darken && pSWExt->CommanderPoints_Amount != 0 &&
								!pOwnerExt->CanTransactCommanderPoints(pSWExt->CommanderPoints_Amount))
							{
								darken = true;
							}
						}
						else
						{
							// Missing house ext? play it safe.
							darken = true;
						}

						// Requirement 4: AuxTechnos / other availability gates
						if (!darken && !pSWExt->IsAvailable(pSW->Owner))
						{
							darken = true;
						}
					}
				}
				else
				{
					// No extension found for this type → disable to avoid edge crashes/misfires
					darken = true;
				}
			}
		}
	}

	// BL = darken flag (engine uses it to shade the cameo)
	R->BL(darken);
	return 0;
}




#pragma endregion
