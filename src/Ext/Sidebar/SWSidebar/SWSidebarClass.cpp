#include "SWSidebarClass.h"
#include <CommandClass.h>

#include <Ext/House/Body.h>
#include <Ext/Side/Body.h>
#include <Ext/SWType/Body.h>
#include "UISafeOps.h"

SWSidebarClass SWSidebarClass::Instance;
CommandClass* SWSidebarClass::Commands[10];

// =============================
// functions

bool SWSidebarClass::AddColumn()
{
	auto& columns = this->Columns;
	const int columnsCount = static_cast<int>(columns.size());

	if (columnsCount >= Phobos::UI::SuperWeaponSidebar_MaxColumns)
		return false;

	const int firstColumn = Phobos::UI::SuperWeaponSidebar_Max;
	const int maxButtons = Phobos::UI::SuperWeaponSidebar_Pyramid ? firstColumn - columnsCount : firstColumn;

	if (maxButtons <= 0)
		return false;

	const int cameoWidth = 60;
	const auto column = GameCreate<SWColumnClass>(maxButtons, 0, 0, cameoWidth + Phobos::UI::SuperWeaponSidebar_Interval, Phobos::UI::SuperWeaponSidebar_CameoHeight);
	column->Zap();
	GScreenClass::Instance.AddButton(column);
	return true;
}

bool SWSidebarClass::RemoveColumn()
{
	auto& columns = this->Columns;

	if (columns.empty())
		return false;

	if (const auto backColumn = columns.back())
	{
		AnnounceInvalidPointer(SWSidebarClass::Instance.CurrentColumn, backColumn);

		// Defer removal of all child buttons first
		for (const auto btn : backColumn->Buttons)
		{
			if (btn) UISafeOps::EnqueueRemoveGadget(btn);
		}
		backColumn->Buttons.clear();

		// Defer the column gadget itself
		UISafeOps::EnqueueRemoveGadget(backColumn);

		columns.erase(columns.end() - 1);
		return true;
	}

	return false;
}

void SWSidebarClass::InitClear()
{
	this->CurrentColumn = nullptr;
	this->CurrentButton = nullptr;

	if (const auto toggleButton = this->ToggleButton)
	{
		this->ToggleButton = nullptr;
		GScreenClass::Instance.RemoveButton(toggleButton);
		GameDelete(toggleButton);
	}

	auto& columns = this->Columns;

	for (const auto column : columns)
	{
		column->ClearButtons();
		GScreenClass::Instance.RemoveButton(column);
		GameDelete(column);
	}

	columns.clear();
}

void SWSidebarClass::InitIO()
{
	if (!Phobos::UI::SuperWeaponSidebar || Unsorted::ArmageddonMode)
		return;

	if (const auto pSideExt = SideExt::ExtMap.TryFind(SideClass::Array.Items[ScenarioClass::Instance->PlayerSideIndex]))
	{
		const auto pOnPCX = pSideExt->SuperWeaponSidebar_OnPCX.GetSurface();
		const auto pOffPCX = pSideExt->SuperWeaponSidebar_OffPCX.GetSurface();
		int width = 0, height = 0;

		if (pOnPCX)
		{
			if (pOffPCX)
			{
				width = std::max(pOnPCX->GetWidth(), pOffPCX->GetWidth());
				height = std::max(pOnPCX->GetHeight(), pOffPCX->GetHeight());
			}
			else
			{
				width = pOnPCX->GetWidth();
				height = pOnPCX->GetHeight();
			}
		}
		else if (pOffPCX)
		{
			width = pOffPCX->GetWidth();
			height = pOffPCX->GetHeight();
		}

		if (width > 0 && height > 0)
		{
			const auto toggleButton = GameCreate<ToggleSWButtonClass>(0, 0, width, height);
			toggleButton->Zap();
			GScreenClass::Instance.AddButton(toggleButton);
			SWSidebarClass::Instance.ToggleButton = toggleButton;
			toggleButton->UpdatePosition();
		}
	}

	for (const auto superIdx : ScenarioExt::Global()->SWSidebar_Indices)
		SWSidebarClass::Instance.AddButton(superIdx);
}

bool SWSidebarClass::AddButton(int superIdx)
{
	if (!Phobos::UI::SuperWeaponSidebar || this->DisableEntry)
		return false;

	const auto pSWType = SuperWeaponTypeClass::Array.GetItemOrDefault(superIdx);

	if (!pSWType)
		return false;

	const auto pSWExt = SWTypeExt::ExtMap.Find(pSWType);

	if (!pSWExt->SW_ShowCameo && pSWExt->SW_AutoFire)
		return false;

	if (!pSWExt->SuperWeaponSidebar_Allow.Get(RulesExt::Global()->SuperWeaponSidebar_AllowByDefault))
		return false;

	const unsigned int ownerBits = 1u << HouseClass::CurrentPlayer->Type->ArrayIndex;

	if ((pSWExt->SuperWeaponSidebar_RequiredHouses & ownerBits) == 0)
		return false;

	if (pSWExt->SuperWeaponSidebar_Significance < Phobos::Config::SuperWeaponSidebar_RequiredSignificance)
		return false;

	// If SW.AuxTechnos.Required=true, only add cameo if requirements are met
	if (pSWExt->SW_AuxTechnos_Required && !pSWExt->IsAvailable(HouseClass::CurrentPlayer))
		return false;

	auto& columns = this->Columns;

	if (columns.empty() && !this->AddColumn())
		return false;

	if (std::ranges::any_of(columns, [superIdx](SWColumnClass* column) { return std::ranges::any_of(column->Buttons, [superIdx](SWButtonClass* button) { return button->SuperIndex == superIdx; }); }))
		return true;

	return columns.back()->AddButton(superIdx);
}

// =============================
// PERF/STABILITY: safe, fast sort (precompute priority once; stable order for ties)
void SWSidebarClass::SortButtons()
{
	auto& columns = this->Columns;

	if (columns.empty())
	{
		if (const auto toggleButton = this->ToggleButton)
			toggleButton->UpdatePosition();

		return;
	}

	// 1) collect all buttons
	std::vector<SWButtonClass*> vec_Buttons;
	vec_Buttons.reserve(this->GetMaximumButtonCount());

	for (const auto column : columns)
	{
		for (const auto button : column->Buttons)
			vec_Buttons.emplace_back(button);

		column->ClearButtons(false);
	}

	// 2) precompute priority for current player once per button (no lookups in comparator)
	const unsigned int ownerBits = 1u << HouseClass::CurrentPlayer->Type->ArrayIndex;

	struct BtnPri { SWButtonClass* btn; uint8_t pri; };
	std::vector<BtnPri> items;
	items.reserve(vec_Buttons.size());

	for (auto* btn : vec_Buttons)
	{
		const int idx = btn->SuperIndex;
		const auto* swt = SuperWeaponTypeClass::Array.GetItemOrDefault(idx);
		uint8_t pri = 0;
		if (const auto* ext = SWTypeExt::ExtMap.TryFind(swt))
		{
			pri = (ext->SuperWeaponSidebar_PriorityHouses & ownerBits) ? 1 : 0;
		}
		items.push_back({ btn, pri });
	}

	// 3) stable sort by (priority desc, BuildType tie-breaker)
	std::stable_sort(items.begin(), items.end(),
		[](const BtnPri& A, const BtnPri& B)
{
	if (A.pri != B.pri) return A.pri > B.pri;
	return BuildType::SortsBefore(AbstractType::Special, A.btn->SuperIndex,
								  AbstractType::Special, B.btn->SuperIndex);
		});

	// 4) write back to vec_Buttons in the new order
	vec_Buttons.clear();
	vec_Buttons.reserve(items.size());
	for (const auto& it : items) vec_Buttons.emplace_back(it.btn);

	// 5) lay out back into columns (original placement logic)
	const auto pTopPCX =
		SideExt::ExtMap.TryFind(SideClass::Array.Items[ScenarioClass::Instance->PlayerSideIndex])
		->SuperWeaponSidebar_TopPCX.GetSurface();

	const int buttonCount = static_cast<int>(vec_Buttons.size());
	const int cameoWidth = 60, cameoHeight = 48;
	const int firstColumn = Phobos::UI::SuperWeaponSidebar_Max;
	const int cameoHarfInterval = (Phobos::UI::SuperWeaponSidebar_CameoHeight - cameoHeight) / 2;
	int location_Y = (DSurface::ViewBounds.Height - std::min(buttonCount, firstColumn) * Phobos::UI::SuperWeaponSidebar_CameoHeight) / 2;
	Point2D location = { Phobos::UI::SuperWeaponSidebar_LeftOffset, location_Y + cameoHarfInterval };
	int rowIdx = 0, columnIdx = 0;

	for (const auto button : vec_Buttons)
	{
		const auto column = columns[columnIdx];

		if (rowIdx == 0)
			column->SetPosition(location.X - Phobos::UI::SuperWeaponSidebar_LeftOffset, location_Y - (pTopPCX ? pTopPCX->GetHeight() : 0));

		column->Buttons.emplace_back(button);
		button->SetColumn(columnIdx);
		button->SetPosition(location.X, location.Y);
		rowIdx++;

		const int currentCapacity = Phobos::UI::SuperWeaponSidebar_Pyramid ? firstColumn - columnIdx : firstColumn;

		if (rowIdx >= currentCapacity)
		{
			rowIdx = 0;
			columnIdx++;

			if (Phobos::UI::SuperWeaponSidebar_Pyramid)
				location_Y += Phobos::UI::SuperWeaponSidebar_CameoHeight / 2;

			location.X += cameoWidth + Phobos::UI::SuperWeaponSidebar_Interval;
			location.Y = location_Y + cameoHarfInterval;
		}
		else
		{
			location.Y += Phobos::UI::SuperWeaponSidebar_CameoHeight;
		}
	}

	for (const auto column : columns)
		column->SetHeight(static_cast<int>(column->Buttons.size()) * Phobos::UI::SuperWeaponSidebar_CameoHeight);

	UISafeOps::ProcessDeferredFromSidebarSort();
}

int SWSidebarClass::GetMaximumButtonCount()
{
	const int firstColumn = Phobos::UI::SuperWeaponSidebar_Max;

	if (Phobos::UI::SuperWeaponSidebar_Pyramid)
	{
		const int columns = std::min(firstColumn, Phobos::UI::SuperWeaponSidebar_MaxColumns);
		return (firstColumn + (firstColumn - (columns - 1))) * columns / 2;
	}

	return firstColumn * Phobos::UI::SuperWeaponSidebar_MaxColumns;
}

bool SWSidebarClass::IsEnabled()
{
	return ScenarioExt::Global()->SWSidebar_Enable;
}

// =============================
// PERF/STABILITY: robust presence checks + bitmap-based re-add
void SWSidebarClass::RecheckCameo()
{
	// Reentrancy guard (function-static so it persists)
	static bool InRecheck = false;
	if (InRecheck) return;
	InRecheck = true;

	// tiny scope guard (no GSL needed)
	struct ScopeExit
	{
		bool& b;
		~ScopeExit() noexcept { b = false; }
	} _reset { InRecheck };

	auto& sidebar = SWSidebarClass::Instance;
	auto& super = HouseClass::CurrentPlayer->Supers;
	bool& recheckTechTree = HouseClass::CurrentPlayer->RecheckTechTree;

	// pass 1: collect removals per column
	for (const auto& column : sidebar.Columns)
	{
		std::vector<int> removeButtons;
		removeButtons.reserve(column->Buttons.size());

		for (const auto& button : column->Buttons)
		{
			if (!button) continue;
			const int idx = button->SuperIndex;
			if (idx < 0 || !super.ValidIndex(idx)) continue;

			// Remove if SuperWeapon is not present
			if (!super[idx]->IsPresent)
			{
				removeButtons.push_back(idx);
				continue;
			}

			// Remove if SW.AuxTechnos.Required = true and requirements not met
			// NOTE: This should remove from SWSidebar to let it fall back to main sidebar
			if (const auto pSWType = SuperWeaponTypeClass::Array.GetItemOrDefault(idx))
			{
				if (const auto pSWExt = SWTypeExt::ExtMap.Find(pSWType))
				{
					if (pSWExt->SW_AuxTechnos_Required && !pSWExt->IsAvailable(HouseClass::CurrentPlayer))
					{
						removeButtons.push_back(idx);
					}
				}
			}
		}

		if (!removeButtons.empty())
			recheckTechTree = true;

		for (const auto index : removeButtons)
		{
			// Check if this is a SW.AuxTechnos.Required removal that should keep tracking
			bool keepTracking = false;
			if (const auto pSWType = SuperWeaponTypeClass::Array.GetItemOrDefault(index))
			{
				if (const auto pSWExt = SWTypeExt::ExtMap.Find(pSWType))
				{
					if (pSWExt->SW_AuxTechnos_Required && super.ValidIndex(index) && super[index]->IsPresent)
					{
						keepTracking = true;
					}
				}
			}

			if (keepTracking)
			{
				// Remove button without affecting tracking list
				auto& buttons = column->Buttons;
				const auto it = std::find_if(buttons.begin(), buttons.end(), [index](SWButtonClass* const button) { return button->SuperIndex == index; });
				if (it != buttons.end())
				{
					AnnounceInvalidPointer(SWSidebarClass::Instance.CurrentButton, *it);
					const auto pButton = *it;
					GScreenClass::Instance.RemoveButton(pButton);
					GameDelete(pButton);
					buttons.erase(it);
				}
			}
			else
			{
				// Use normal RemoveButton which removes from tracking list
				column->RemoveButton(index);
			}
		}
	}

	// pass 2: re-add missing cameos efficiently using a presence bitmap
	std::vector<uint8_t> present(SuperWeaponTypeClass::Array.Count, 0);
	for (const auto& column : sidebar.Columns)
	{
		for (const auto& button : column->Buttons)
		{
			if (!button) continue;
			const int idx = button->SuperIndex;
			if (idx >= 0 && idx < SuperWeaponTypeClass::Array.Count)
				present[idx] = 1;
		}
	}

	for (const auto superIdx : ScenarioExt::Global()->SWSidebar_Indices)
	{
		if (superIdx >= 0 && superIdx < SuperWeaponTypeClass::Array.Count && !present[superIdx])
		{
			// Check if SW is present and requirements are met before re-adding
			if (super.ValidIndex(superIdx) && super[superIdx]->IsPresent)
			{
				if (const auto pSWType = SuperWeaponTypeClass::Array.GetItemOrDefault(superIdx))
				{
					if (const auto pSWExt = SWTypeExt::ExtMap.Find(pSWType))
					{
						// Only add if SW.AuxTechnos.Required is false OR requirements are met
						if (!pSWExt->SW_AuxTechnos_Required || pSWExt->IsAvailable(HouseClass::CurrentPlayer))
						{
							sidebar.AddButton(superIdx);
						}
					}
					else
					{
						// No extension found, add normally
						sidebar.AddButton(superIdx);
					}
				}
			}
		}
	}

	// re-layout & prune empty columns
	sidebar.SortButtons();

	int removes = 0;
	for (const auto& column : sidebar.Columns)
	{
		if (column->Buttons.empty())
			++removes;
	}
	for (; removes > 0; --removes)
		sidebar.RemoveColumn();

	if (const auto toggleButton = sidebar.ToggleButton)
		toggleButton->UpdatePosition();
}

// Hooks

DEFINE_HOOK(0x4F92FB, HouseClass_UpdateTechTree_SWSidebar, 0x7)
{
	enum { SkipGameCode = 0x4F9302 };

	GET(HouseClass*, pHouse, ESI);

	pHouse->AISupers();

	if (pHouse->IsCurrentPlayer())
	{
		SWSidebarClass::RecheckCameo();
		
		// Also recheck main sidebar for SW.AuxTechnos.Required cameos that might need to be re-added
		auto pSidebar = &SidebarClass::Instance;
		for (int i = 0; i < pHouse->Supers.Count; i++)
		{
			if (auto pSuper = pHouse->Supers.GetItem(i))
			{
				if (pSuper->IsPresent)
				{
					if (auto pSWExt = SWTypeExt::ExtMap.Find(pSuper->Type))
					{
						// Check if this SW has AuxTechnos requirement and is now available
						if (pSWExt->SW_AuxTechnos_Required && pSWExt->IsAvailable(pHouse))
						{
							// Check if it's not already in main sidebar and not in SWSidebar
							bool inMainSidebar = false;
							for (int tab = 0; tab < 4 && !inMainSidebar; tab++)
							{
								auto& strip = pSidebar->Tabs[tab];
								for (int btn = 0; btn < strip.CameoCount && !inMainSidebar; btn++)
								{
									auto& cameo = strip.Cameos[btn];
									if (cameo.ItemType == AbstractType::Special && cameo.ItemIndex == i)
									{
										inMainSidebar = true;
									}
								}
							}
							
							// If not in main sidebar and not successfully added to SWSidebar, try to add to main sidebar
							if (!inMainSidebar && !SWSidebarClass::Instance.AddButton(i))
							{
								pSidebar->AddCameo(AbstractType::Special, i);
							}
						}
					}
				}
			}
		}
	}

	return SkipGameCode;
}

DEFINE_HOOK(0x6A6316, SidebarClass_AddCameo_SuperWeapon_SWSidebar, 0x6)
{
	enum { ReturnFalse = 0x6A65FF };

	GET_STACK(AbstractType, whatAmI, STACK_OFFSET(0x14, 0x4));

	if (whatAmI != AbstractType::Special && whatAmI != AbstractType::SuperWeaponType && whatAmI != AbstractType::Super)
		return 0;

	GET_STACK(const int, index, STACK_OFFSET(0x14, 0x8));

	// Always track eligible superweapons for rechecking
	bool shouldTrackInSWSidebar = false;
	bool shouldHideFromMainSidebar = false;

	if (auto pPlayer = HouseClass::CurrentPlayer)
	{
		if (auto pSuper = pPlayer->Supers.GetItemOrDefault(index))
		{
			if (auto pSWExt = SWTypeExt::ExtMap.Find(pSuper->Type))
			{
				// Check if this SW should be tracked by SWSidebar (basic eligibility)
				if (pSWExt->SuperWeaponSidebar_Allow.Get(RulesExt::Global()->SuperWeaponSidebar_AllowByDefault))
				{
					const unsigned int ownerBits = 1u << pPlayer->Type->ArrayIndex;
					if ((pSWExt->SuperWeaponSidebar_RequiredHouses & ownerBits) != 0 && 
						pSWExt->SuperWeaponSidebar_Significance >= Phobos::Config::SuperWeaponSidebar_RequiredSignificance)
					{
						shouldTrackInSWSidebar = true;
					}
				}

				// Check if should be hidden from main sidebar due to SW.AuxTechnos.Required
				if (pSWExt->SW_AuxTechnos_Required && !pSWExt->IsAvailable(pPlayer))
				{
					shouldHideFromMainSidebar = true;
				}
			}
		}
	}

	if (shouldTrackInSWSidebar)
	{
		// Always add to tracking list for rechecking
		auto& indices = ScenarioExt::Global()->SWSidebar_Indices;
		if (std::find(indices.begin(), indices.end(), index) == indices.end())
		{
			indices.emplace_back(index);
		}

		// Try to add to SWSidebar if requirements are met
		if (SWSidebarClass::Instance.AddButton(index))
		{
			return ReturnFalse;
		}
		// If SWSidebar rejected it but requirements are met, fall through to main sidebar
	}

	// Hide from main sidebar if SW.AuxTechnos.Required and requirements not met
	if (shouldHideFromMainSidebar)
	{
		return ReturnFalse;
	}

	return 0;
}

DEFINE_HOOK(0x6AA790, StripClass_RecheckCameo_RemoveCameo, 0x6)
{
	enum { ShouldRemove = 0x6AA7B6, ShouldNotRemove = 0x6AAA68 };

	GET(BuildType*, pItem, ESI);
	const auto pCurrent = HouseClass::CurrentPlayer;
	const auto& supers = pCurrent->Supers;

	// Only handle superweapons
	if (pItem->ItemType != AbstractType::Special)
		return 0;

	if (supers.ValidIndex(pItem->ItemIndex) && supers[pItem->ItemIndex]->IsPresent)
	{
		// Check if we should remove from main sidebar for SW.AuxTechnos.Required
		if (auto pSuper = supers[pItem->ItemIndex])
		{
			if (auto pSWExt = SWTypeExt::ExtMap.Find(pSuper->Type))
			{
				if (pSWExt->SW_AuxTechnos_Required && !pSWExt->IsAvailable(pCurrent))
				{
					// Remove from main sidebar since requirements not met
					return ShouldRemove;
				}
			}
		}

		// Try to add to SWSidebar if possible
		if (SWSidebarClass::Instance.AddButton(pItem->ItemIndex))
		{
			// Successfully added to SWSidebar, track it and remove from main sidebar
			auto& indices = ScenarioExt::Global()->SWSidebar_Indices;
			if (std::find(indices.begin(), indices.end(), pItem->ItemIndex) == indices.end())
			{
				indices.emplace_back(pItem->ItemIndex);
			}
			return ShouldRemove;
		}
		else
		{
			// Couldn't add to SWSidebar, keep in main sidebar
			return ShouldNotRemove;
		}
	}

	return ShouldRemove;
}
