#include "ShowTechnoNames.h"

#include <Utilities/GeneralUtils.h>
#include <Phobos.h>

const char* ShowTechnoNamesCommandClass::GetName() const
{
	return "Show Techno Names";
}

const wchar_t* ShowTechnoNamesCommandClass::GetUIName() const
{
	return GeneralUtils::LoadStringUnlessMissing("TXT_SHOW_TECHNO_NAMES", L"Show Techno Names");
}

const wchar_t* ShowTechnoNamesCommandClass::GetUICategory() const
{
	return CATEGORY_DEVELOPMENT;
}

const wchar_t* ShowTechnoNamesCommandClass::GetUIDescription() const
{
	return GeneralUtils::LoadStringUnlessMissing("TXT_SHOW_TECHNO_NAMES_DESC", L"Display TechnoType names on all units & buildings for debugging.");
}

void ShowTechnoNamesCommandClass::Execute(WWKey eInput) const
{
	Phobos::DisplayTechnoNames = !Phobos::DisplayTechnoNames;
}