#include <New/Entity/AttachmentClass.h>

#include <Utilities/Macro.h>

// This hook was moved to src/Ext/Scenario/Body.cpp at 0x55B4E1 because
// the 0x55B6B3 address became invalid after rebase - causing attachments to not move
// DEFINE_HOOK(0x55B6B3, LogicClass_AI_After, 0x5)
// {
// 	for (auto const& attachment : AttachmentClass::Array)
// 		attachment->AI();
//
// 	return 0;
// }
