#include "PapyrusBetterCriticals.h"
#include "f4se/PapyrusNativeFunctions.h"



namespace PapyrusBetterCriticals
{
	// native HasKeyword check
	bool HasKeyword_Native(IKeywordFormBase * keywordBase, BGSKeyword * checkKW)
	{
		if (!checkKW || !keywordBase) {
			return false;
		}
		auto HasKeyword_Internal = GetVirtualFunction<_IKeywordFormBase_HasKeyword>(keywordBase, 1);
		if (HasKeyword_Internal(keywordBase, checkKW, 0)) {
			return true;
		}
		return false;
	}


	// finds the appropriate crit effect table and performs a roll to get a critical effect spell
	SpellItem * GetCritEffect(StaticFunctionTag*, Actor * source, Actor * target, TESForm * critIDForm)
	{
		_MESSAGE("GetCritEffect called");
		if (!source || !target || !critIDForm) {
			_MESSAGE("  ERROR: GetCritSpell:  No Source, Target or critIDForm!");
			return nullptr;
		}
		
		// get the Actor type's tables
		int critTableType = (int)target->actorValueOwner.GetValue(BCGlobals::avCritEffectsType);
		_MESSAGE("GetCritEffect: got the critTableType (%i)", critTableType);
		if (critTableType < 1) {
			return nullptr;
		}
		if (critTableType >= BCGlobals::critEffectTables.size()) {
			_MESSAGE("  WARNING: GetCritSpell:  invalid critTableType!");
			return nullptr;
		}
		// check if the target is wearing power armor
		if (target->race->formID == 0x13746 || target->race->formID == 0xEAFB6) {
			if (target->equipData) {
				if ((target->equipData->kMaxSlots > 3) && target->equipData->slots[3].item) {
					TESObjectARMO * bodyArmor = reinterpret_cast<TESObjectARMO*>(target->equipData->slots[3].item);
					if (bodyArmor) {
						if (HasKeyword_Native(&bodyArmor->keywordForm.keywordBase, BCGlobals::kwIsPowerArmorFrame)) {
							//critTableType = BCGlobals::critEffectTables.size() - 1;
							return nullptr;
						}
					}
				}
			}
		}
		_MESSAGE("GetCritEffect: checked equipData");
		CritEffectTable targetCritTable = BCGlobals::critEffectTables[critTableType];
		_MESSAGE("  Target Crit Table: %s", targetCritTable.sMenuName.c_str());

		// get the damage type's table
		std::vector<CritEffectTable::TypedCritTable>::iterator typedCritTableIt = std::find_if(targetCritTable.critEffects_Typed.begin(), targetCritTable.critEffects_Typed.end(), FindPred_TypedCritTable_ByID(critIDForm->formID));
		if (typedCritTableIt == targetCritTable.critEffects_Typed.end()) {
			_MESSAGE("  WARNING: GetCritSpell:  no table for this damageType index!");
			return nullptr;
		}

		std::vector<CritEffectTable::CritEffect> curEffectTable = typedCritTableIt->critEffects;
		if (curEffectTable.empty()) {
			_MESSAGE("  WARNING: GetCritSpell:  curEffectTable is empty!");
			return nullptr;
		}

		// get the crit effect for the roll value
		int iCritRollMod = (int)source->actorValueOwner.GetValue(BCGlobals::avCritRollMod);
		int iCritEffectRoll = BCGlobals::rng.RandomInt(0, 85 + iCritRollMod);
		_MESSAGE("  Critical Effect Roll:  %i", iCritEffectRoll);

		std::vector<CritEffectTable::CritEffect>::iterator critEffectIt = std::find_if(curEffectTable.begin(), curEffectTable.end(), FindPred_CritEffect_ByRoll(iCritEffectRoll));
		if (critEffectIt == curEffectTable.end()) {
			_MESSAGE("  WARNING: GetCritSpell:  no effect for this roll value!");
			return nullptr;
		}

		if ((critEffectIt->critEffectID != -1) && (critEffectIt->critEffectID < BCGlobals::critEffectSpells.size())) {
			SpellItem * critSpell = nullptr;
			if (BCGlobals::bCritsUseSavingRoll && (critEffectIt->iSavingRollAVIndex > -1)) {
				// saving roll
				int iSaveVal = (int)target->actorValueOwner.GetValue(BCGlobals::specialAVs[critEffectIt->iSavingRollAVIndex]);
				int iSavingRoll = 0;
				if (iSaveVal > 0) {
					iSavingRoll = BCGlobals::rng.RandomInt(0, iSaveVal);
				}
				_MESSAGE("    Saving roll: %i/%i", iSavingRoll, iSaveVal);
				if (iSavingRoll < critEffectIt->iSavingRollPass) {
					_MESSAGE("    Crit effect index: %i", critEffectIt->critEffectID);
					critSpell = BCGlobals::critEffectSpells[critEffectIt->critEffectID];
					if (critSpell) {
						_MESSAGE("    Saving roll failed - Critical Effect: %s,  target: %s\n", critSpell->name.name.c_str(), target->baseForm->GetFullName());
						return critSpell;
					}
				}
				else {
					// lesser crit effect
					_MESSAGE("    Saved effect index: %i", critEffectIt->savedCritEffectID);
					if ((critEffectIt->savedCritEffectID != -1) && (critEffectIt->savedCritEffectID < BCGlobals::critEffectSpells.size())) {
						critSpell = BCGlobals::critEffectSpells[critEffectIt->savedCritEffectID];
						if (critSpell) {
							_MESSAGE("    Saving roll passed - Critical Effect: %s,  target: %s\n", critSpell->name.name.c_str(), target->baseForm->GetFullName());
							return critSpell;
						}
					}
					else {
						_MESSAGE("    Saving roll passed - Critical Effect: none,  target: %s\n", target->baseForm->GetFullName());
						return nullptr;
					}
				}
			}
			else {
				critSpell = BCGlobals::critEffectSpells[critEffectIt->critEffectID];
				if (critSpell) {
					_MESSAGE("    Critical Effect: %s,  target: %s\n", critSpell->name.name.c_str(), target->baseForm->GetFullName());
					return critSpell;
				}
			}
		}
		_MESSAGE("    Critical Effect: none,  target: %s\n", target->baseForm->GetFullName());
		return nullptr;
	}


	void CalcWeaponCriticalChance(StaticFunctionTag*, Actor * target)
	{
		if (!target) {
			_MESSAGE("ERROR: CalcWeaponCriticalChance missing target!");
			return;
		}


	}

}


bool PapyrusBetterCriticals::RegisterPapyrus(VirtualMachine* vm)
{
	RegisterFuncs(vm);
	return true;
}


void PapyrusBetterCriticals::RegisterFuncs(VirtualMachine* vm)
{
	vm->RegisterFunction(
		new NativeFunction3 <StaticFunctionTag, SpellItem*, Actor*, Actor*, TESForm*>("GetCritEffect", SCRIPTNAME_Globals, GetCritEffect, vm));
	
	_MESSAGE("Registered native functions for %s", SCRIPTNAME_Globals);
}
