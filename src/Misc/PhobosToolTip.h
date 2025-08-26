#pragma once

#include <SidebarClass.h>
#include <SuperWeaponTypeClass.h>
#include <TechnoTypeClass.h>

#include <Phobos.h>

#include <Ext/TechnoType/Body.h>
#include <Ext/SWType/Body.h>

#include <string>

struct StripClass;

class PhobosToolTip
{
public:
	static PhobosToolTip Instance;

private:
	inline const wchar_t* GetUIDescription(TechnoTypeExt::ExtData* pData) const;
	inline const wchar_t* GetUIDescription(SWTypeExt::ExtData* pData) const;
	inline int GetBuildTime(TechnoTypeClass* pType) const;
	inline int GetPower(TechnoTypeClass* pType) const;

	// buffer helpers
	static constexpr std::size_t BufferCap = 512;
	inline void ClearBuffer();
	inline void Append(const wchar_t* s);
	inline void AppendInt(int v);
	inline void AppendFixed2(int mm, int ss);
	inline const wchar_t* Buf() const { return this->TextBuffer; }

public:
	inline bool IsEnabled() const;
	inline const wchar_t* GetBuffer() const { return this->Buf(); }

	void HelpText(BuildType& cameo);
	void HelpText_Techno(TechnoTypeClass* pType);
	void HelpText_Super(int swidx);

	// Properties
private:
	wchar_t TextBuffer[BufferCap]; // C buffer
	std::size_t Len;

public:
	bool IsCameo { false };
	bool SlaveDraw { false };
};
