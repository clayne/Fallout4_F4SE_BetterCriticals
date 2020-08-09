#pragma once
#include "f4se/GameData.h"
#include "f4se/GameObjects.h"
#include "f4se/GameReferences.h"
#include "f4se/PapyrusVM.h"

#include <shlobj.h>
#include <string>
#include <set>
#include <vector>
#include <algorithm>

#include "RNG.h"



// BGSExplosion
// 144?
class TempBGSExplosion : public TESBoundObject
{
public:
	enum { kTypeID = kFormType_EXPL };

	TESFullName				fullName;				// 68
	TESModel				model;					// 78
	BGSPreloadable			preloadable;			// A8

	EnchantmentItem *		objectEffect;			// B0
	UInt64					unkB8;					// B8
	UInt64					unkC0;					// C0
	UInt64					unkC8;					// C8

	TESImageSpaceModifier * imageSpaceModifier;		// D0

	// 6C
	struct ExplosionData
	{
		TESForm					* light;			// 00
		BGSSoundDescriptorForm	* sound1;			// 08
		BGSSoundDescriptorForm	* sound2;			// 10
		TESForm					* impactDataSet;	// 18
		TESForm					* placedObject;		// 20

		BGSProjectile			* spawnProjectile;	// 28
		float					spawnX;				// 30
		float					spawnY;				// 34
		float					spawnZ;				// 38
		float					spawnSpreadDeg;		// 3C
		UInt32					spawnCount;			// 40

		float					force;				// 44
		float					damage;				// 48
		float					innerRadius;		// 4C
		float					outerRadius;		// 50
		float					isRadius;			// 54
		float					verticalOffset;		// 58

		UInt32					flags;				// 5C
		UInt32					soundLevel;			// 60
		float					placedObjFadeDelay;	// 64
		UInt32					stagger;			// 68
	};

	ExplosionData			data;					// D8
};


/* TESSpellList's unk08
- used by TESRace and TESActorBase for default abilities
- edits need to be done early (or require reloading a save) since TESRace, Actor are used as templates by the game
1C or 20? */
class TempSpellListEntries
{
public:
	SpellItem			** spells;				//00 - array of SpellItems
	void				* unk08;				//08
	void				* unk10;				//10
	UInt32				numSpells;				//18 - length of the spells array - set manually
};



class CritEffectTable
{
public:
	struct CritEffect
	{
		int				iRollMax =				255;		// upper threshold for the crit roll (search key)
		int				critEffectID =			-1;			// critical effect spell ID
		int				savedCritEffectID =		-1;			// critical effect ID to use if saving roll passed
		int				iSavingRollAVIndex =	-1;			// ActorValue to check for in the saving roll (-1 for none)
		int				iSavingRollPass =		0;			// amount needed to pass the saving roll
	};

	struct TypedCritTable
	{
		int				iCritTableID =			0;			// formID used to identify crit type (damage type, weapon, keyword, etc.)
		std::string		sMenuName =				"";
		std::vector<CritEffect>	critEffects;
	};

	std::string		sMenuName = "";
	std::vector<TypedCritTable>	critEffects_Typed;		// damageType-based crit tables
};

// search predicate for damage type crit tables
struct FindPred_TypedCritTable_ByID
{
	int tableID;
	FindPred_TypedCritTable_ByID(int tableID) : tableID(tableID) {}
	bool operator () (const CritEffectTable::TypedCritTable & critTable) const
	{
		return (tableID == critTable.iCritTableID);
	}
};

// search predicate for crit effects by roll value
struct FindPred_CritEffect_ByRoll
{
	int iRoll;
	FindPred_CritEffect_ByRoll(UInt32 iRoll) : iRoll(iRoll) {}
	bool operator () (const CritEffectTable::CritEffect & critEffect) const
	{
		return (iRoll <= critEffect.iRollMax);
	}
};

/** native HasKeyword/GetVirtualFunction
		credit: shavkacagarikia (https://github.com/shavkacagarikia/ExtraItemInfo) **/
typedef bool(*_IKeywordFormBase_HasKeyword)(IKeywordFormBase* keywordFormBase, BGSKeyword* keyword, UInt32 unk3);

template <typename T>
T GetVirtualFunction(void* baseObject, int vtblIndex) {
	uintptr_t* vtbl = reinterpret_cast<uintptr_t**>(baseObject)[0];
	return reinterpret_cast<T>(vtbl[vtblIndex]);
}


class BCGlobals
{
public:
	static ATxoroshiro128p rng;

	static std::vector<SpellItem*> critEffectSpells;
	static std::vector<CritEffectTable> critEffectTables;
	static std::vector<ActorValueInfo*> specialAVs;

	static ActorValueInfo * avCritChance;
	static ActorValueInfo * avCritChanceMult;
	static ActorValueInfo * avCritRollMod;
	static ActorValueInfo * avCritEffectsType;

	static BGSKeyword * kwIsPowerArmorFrame;

	static bool bCritChanceUseCritChargeMult;
	static bool bCritChanceUseWpnCND;

	static bool bCritsUseSavingRoll;

};


namespace BetterCriticals
{
	bool LoadConfigData();
}


namespace PapyrusBetterCriticals
{
	bool RegisterPapyrus(VirtualMachine * vm);
}
