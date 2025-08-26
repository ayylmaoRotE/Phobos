#pragma region Ares Copyrights
/*
 *Copyright (c) 2008+, All Ares Contributors
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *3. All advertising materials mentioning features or use of this software
 *   must display the following acknowledgement:
 *   This product includes software developed by the Ares Contributors.
 *4. Neither the name of Ares nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY ITS CONTRIBUTORS ''AS IS'' AND ANY
 *EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *DISCLAIMED. IN NO EVENT SHALL THE ARES CONTRIBUTORS BE LIABLE FOR ANY
 *DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma endregion

#include "Phobos.h"

#include <Ext/Rules/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/Techno/Body.h>
#include <Ext/Building/Body.h>

// AI targeting of Iron Curtained units
DEFINE_HOOK(0x6FC22A, TechnoClass_GetFireError_AttackICUnit, 0x6)
{
	enum { ContinueCheck = 0x6FC23A, BypassCheck = 0x6FC24D };
	GET(TechnoClass* const, pThis, ESI);

	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis->GetTechnoType());
	// Allow AI to attack IC'd targets if enabled globally or per-unit
	const bool Allow = RulesExt::Global()->AutoAttackICedTarget || pThis->Owner->IsControlledByHuman();
	return pTypeExt->AllowFire_IroncurtainedTarget.Get(Allow)
		? BypassCheck : ContinueCheck;
}

// Dummy TechnoType - skip "behind object" mouse cursor
// DISABLED: Hook address/approach needs research for RotE compatibility  
/*DEFINE_HOOK(0x6FA2CF, TechnoClass_AI_DrawBehindAnim, 0x9)
{
	GET(TechnoClass*, pThis, ESI);
	GET(Point2D*, pPoint, ECX);
	GET(RectangleStruct*, pBound, EAX);

	// Safety checks first
	if (!pThis || !pThis->GetTechnoType())
	{
		// Call original function if we can't get the type
		pThis->DrawBehind(pPoint, pBound);
		return 0x6FA30C;
	}

	// Get TechnoType extension with safety check
	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis->GetTechnoType());
	if (!pTypeExt)
	{
		// Extension not found, call original function
		pThis->DrawBehind(pPoint, pBound);
		return 0x6FA30C;
	}

	// ONLY skip DrawBehind for units explicitly set as Dummy=yes
	if (pTypeExt->IsDummy)
	{
		// Skip drawing for dummy units
		return 0x6FA30C;
	}

	// For all other cases, check original conditions and call DrawBehind

	// Check for Building in limbo
	if (const auto pBld = abstract_cast<BuildingClass*>(pThis))
	{
		const auto pBldExt = BuildingExt::ExtMap.Find(pBld);
		if (pBldExt && pBldExt->LimboID != -1)
		{
			return 0x6FA30C;
		}
	}

	// Check if in transport
	if (pThis->InOpenToppedTransport)
		return 0x6FA30C;

	// Normal case - call the original DrawBehind function
	pThis->DrawBehind(pPoint, pBound);

	return 0x6FA30C;
}*/
