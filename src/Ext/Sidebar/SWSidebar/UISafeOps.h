#pragma once

class GadgetClass;  // global forward decl

namespace UISafeOps
{
	void SetPerFrameBudget(int adds) noexcept;
	void EnqueueAddCameo(int swIndex) noexcept;

	// NOTE the leading :: to force global GadgetClass
	void EnqueueRemoveGadget(::GadgetClass* g) noexcept;
	void ProcessDeferredFromSidebarSort() noexcept;
}
