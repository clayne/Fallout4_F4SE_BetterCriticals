#pragma once
#include "BetterCriticals.h"

#define SCRIPTNAME_Globals "BetterCriticals:Globals"


struct StaticFunctionTag;
class VirtualMachine;


namespace PapyrusBetterCriticals
{
	void RegisterFuncs(VirtualMachine* vm);
}

