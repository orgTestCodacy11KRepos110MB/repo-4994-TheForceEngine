#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "vmConfig.h"

#ifdef VM_ENABLE
namespace TFE_ForceScript
{
	bool init();
	void destroy();

	s32 test();
}
#endif