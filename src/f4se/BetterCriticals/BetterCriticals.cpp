#include "BetterCriticals.h"
#include "nlohmann/json.hpp"
#include <Windows.h>
#include <fstream>
#include <iostream>


std::vector<SpellItem*> BCGlobals::critEffectSpells = std::vector<SpellItem*>();
std::vector<CritEffectTable> BCGlobals::critEffectTables = std::vector<CritEffectTable>();
std::vector<ActorValueInfo*> BCGlobals::specialAVs = std::vector<ActorValueInfo*>();

ATxoroshiro128p BCGlobals::rng = ATxoroshiro128p();

ActorValueInfo * BCGlobals::avCritChance = nullptr;
ActorValueInfo * BCGlobals::avCritRollMod = nullptr;
ActorValueInfo * BCGlobals::avCritEffectsType = nullptr;

BGSKeyword * BCGlobals::kwIsPowerArmorFrame = nullptr;

bool BCGlobals::bCritsUseSavingRoll = false;



using json = nlohmann::json;


// returns a formID from a formatted string
inline UInt32 GetFormIDFromIdentifier(const std::string & formIdentifier)
{
	UInt32 formId = 0;
	if (formIdentifier.c_str() != "none") {
		std::size_t pos = formIdentifier.find_first_of("|");
		std::string modName = formIdentifier.substr(0, pos);
		std::string modForm = formIdentifier.substr(pos + 1);
		sscanf_s(modForm.c_str(), "%X", &formId);

		if (formId != 0x0) {
			UInt8 modIndex = (*g_dataHandler)->GetLoadedModIndex(modName.c_str());
			if (modIndex != 0xFF) {
				formId |= ((UInt32)modIndex) << 24;
			}
			else {
				UInt16 lightModIndex = (*g_dataHandler)->GetLoadedLightModIndex(modName.c_str());
				if (lightModIndex != 0xFFFF) {
					formId |= 0xFE000000 | (UInt32(lightModIndex) << 12);
				}
				else {
					_MESSAGE("ERROR: FormID %s not found!", formIdentifier.c_str());
					formId = 0;
				}
			}
		}
	}
	return formId;
}

// returns a form from a formatted string
inline TESForm * GetFormFromIdentifier(const std::string & formIdentifier)
{
	UInt32 formId = 0;
	if (formIdentifier.c_str() != "none") {
		std::size_t pos = formIdentifier.find_first_of("|");
		std::string modName = formIdentifier.substr(0, pos);
		std::string modForm = formIdentifier.substr(pos + 1);
		sscanf_s(modForm.c_str(), "%X", &formId);
		if (formId != 0x0) {
			UInt8 modIndex = (*g_dataHandler)->GetLoadedModIndex(modName.c_str());
			if (modIndex != 0xFF) {
				formId |= ((UInt32)modIndex) << 24;
			}
			else {
				UInt16 lightModIndex = (*g_dataHandler)->GetLoadedLightModIndex(modName.c_str());
				if (lightModIndex != 0xFFFF) {
					formId |= 0xFE000000 | (UInt32(lightModIndex) << 12);
				}
				else {
					_MESSAGE("ERROR: FormID %s not found!", formIdentifier.c_str());
					formId = 0;
				}
			}
		}
	}
	return (formId != 0x0) ? LookupFormByID(formId) : nullptr;
}

// returns a list of json file names at the passed path
inline std::vector<std::string> GetFileNames(const std::string & folder)
{
	std::vector<std::string> names;
	std::string search_path = folder + "/*.json";
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				names.push_back(fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return names;
}


bool BuildCriticalEffectTableSet(const std::string & dataPath, CritEffectTable & newCritTable)
{
	json cetData;
	std::ifstream loadedFile(dataPath.c_str());
	loadedFile >> cetData;
	loadedFile.close();
	if (cetData.is_null()) {
		_MESSAGE("    ERROR: failed to load file (%s)!", dataPath.c_str());
		return false;
	}
	if (cetData["critTables"].is_null() || cetData["critTables"].empty()) {
		_MESSAGE("    ERROR: File is missing critTables! (%s)", dataPath.c_str());
		return false;
	}

	std::string tableName = cetData["name"];
	_MESSAGE("\n    Creating critical effect tables set: %s...", tableName.c_str());
	newCritTable.sMenuName = tableName.c_str();

	json varTableObj;
	for (json::iterator varTableIt = cetData["critTables"].begin(); varTableIt != cetData["critTables"].end(); ++varTableIt) {
		varTableObj = *varTableIt;
		if (varTableObj.is_null() || varTableObj.empty()) {
			_MESSAGE("      ERROR: Empty or null critTable!");
			continue;
		}
		if (varTableObj["critTableID"].is_null()) {
			_MESSAGE("      ERROR: missing critTableID!");
			continue;
		}
		if (varTableObj["effects"].is_null() || varTableObj["effects"].empty()) {
			_MESSAGE("      ERROR: Empty or null effects list!");
			continue;
		}

		CritEffectTable::TypedCritTable newVarTable = CritEffectTable::TypedCritTable();

		std::string critTypeFormID = varTableObj["critTableID"];
		newVarTable.iCritTableID = GetFormIDFromIdentifier(critTypeFormID);
		if (newVarTable.iCritTableID == 0) {
			_MESSAGE("      ERROR: critTableID not found!");
			continue;
		}

		if (!varTableObj["name"].is_null()) {
			std::string critVarNameStr = varTableObj["name"];
			newVarTable.sMenuName = critVarNameStr.c_str();
		}

		json varCritObj;
		for (json::iterator varCritsIt = varTableObj["effects"].begin(); varCritsIt != varTableObj["effects"].end(); ++varCritsIt) {
			varCritObj = *varCritsIt;
			if (varCritObj.is_null() || varCritObj.empty()) {
				_MESSAGE("      ERROR: Empty or null crit effect data!");
				continue;
			}
			if (varCritObj["rollMax"].is_null()) {
				_MESSAGE("      ERROR: Missing rollMax!");
				continue;
			}

			CritEffectTable::CritEffect newCritEffect = CritEffectTable::CritEffect();
			newCritEffect.iRollMax = varCritObj["rollMax"];

			if (!varCritObj["critEffect"].is_null()) {
				newCritEffect.critEffectID = varCritObj["critEffect"];
			}

			// saving roll stuff - only read if saving rolls are enabled, sSavingRollAV exists and is a valid AV
			if (BCGlobals::bCritsUseSavingRoll) {
				if (!varCritObj["savingRollAV"].is_null()) {
					newCritEffect.iSavingRollAVIndex = varCritObj["savingRollAV"];
					if (newCritEffect.iSavingRollAVIndex != -1) {
						if (!varCritObj["savedEffect"].is_null()) {
							newCritEffect.savedCritEffectID = varCritObj["savedEffect"];
						}
						if (!varCritObj["savingRollPass"].is_null()) {
							newCritEffect.iSavingRollPass = varCritObj["savingRollPass"];
						}
					}
				}
			}

			newVarTable.critEffects.push_back(newCritEffect);
		}
		newCritTable.critEffects_Typed.push_back(newVarTable);
		_MESSAGE("        %s - %i effects", newVarTable.sMenuName, newVarTable.critEffects.size());
	}
	return true;
}


bool LoadDistribution_RaceCritTableSets(const std::string & dataPath)
{
	json distData;
	std::ifstream loadedFile(dataPath.c_str());
	loadedFile >> distData;
	loadedFile.close();
	if (distData.is_null()) {
		_MESSAGE("    ERROR: failed to load distribution file (%s)!", dataPath.c_str());
		return false;
	}

	// race crit table ID distribution
	if (!distData["raceCritTables"].is_null() && !distData["raceCritTables"].empty()) {
		_MESSAGE("\nDistributing race crit table IDs...");
		json racesObj;
		for (json::iterator varRacesIt = distData["raceCritTables"].begin(); varRacesIt != distData["raceCritTables"].end(); ++varRacesIt) {
			racesObj = *varRacesIt;
			if (racesObj.is_null() || racesObj.empty()) {
				_MESSAGE("    WARNING: racesObj is null!");
				continue;
			}
			std::string raceID = racesObj["formID"];
			TESRace * raceForm = (TESRace*)GetFormFromIdentifier(raceID);
			if (!raceForm) {
				_MESSAGE("    WARNING: raceForm %s is null!", raceID.c_str());
				continue;
			}
			if (!raceForm->propertySheet.sheet) {
				raceForm->propertySheet.sheet = new tArray<BGSPropertySheet::AVIFProperty>();
			}
			BGSPropertySheet::AVIFProperty newAVProp;
			newAVProp.actorValue = BCGlobals::avCritEffectsType;
			int critEffectsTypeAmt = racesObj["critTableID"];
			newAVProp.value = (float)critEffectsTypeAmt;
			raceForm->propertySheet.sheet->Push(newAVProp);
			_MESSAGE("    0x%08X: %i", raceForm->formID, critEffectsTypeAmt);
		}
	}

	// race unarmed spells
	if (!distData["unarmedCritSpells"].is_null() && !distData["unarmedCritSpells"].empty()) {
		_MESSAGE("\nDistributing race unarmed crit spells...");
		json racesObj;
		for (json::iterator varRacesIt = distData["unarmedCritSpells"].begin(); varRacesIt != distData["unarmedCritSpells"].end(); ++varRacesIt) {
			racesObj = *varRacesIt;
			if (racesObj.is_null() || racesObj.empty()) {
				_MESSAGE("    WARNING: racesObj is null!");
				continue;
			}
			std::string raceID = racesObj["race"];
			TESRace * raceForm = (TESRace*)GetFormFromIdentifier(raceID);
			if (!raceForm) {
				_MESSAGE("    WARNING: raceForm %s is null!", raceID.c_str());
				continue;
			}
			std::string spellIDStr = racesObj["perkSpell"];
			SpellItem * spellForm = reinterpret_cast<SpellItem*>(GetFormFromIdentifier(spellIDStr));
			if (!spellForm) {
				_MESSAGE("    WARNING: spellForm %s is null!", spellIDStr.c_str());
				continue;
			}
			TempSpellListEntries * spellData = nullptr;
			if (!raceForm->spellList.unk08) {
				raceForm->spellList.unk08 = new TempSpellListEntries();
			}
			spellData = reinterpret_cast<TempSpellListEntries*>(raceForm->spellList.unk08);
			
			// add any current spells to the list
			std::vector<SpellItem*> spellsIn;
			if (spellData->numSpells != 0) {
				for (UInt32 i = 0; i < spellData->numSpells; i++) {
					spellsIn.push_back(spellData->spells[i]);
				}
			}
			spellsIn.push_back(spellForm);

			// rebuild the spells list
			spellData->spells = new SpellItem*[spellsIn.size()];
			spellData->numSpells = spellsIn.size();
			UInt32 spellIdx = 0;
			for (std::vector<SpellItem*>::iterator itAdd = spellsIn.begin(); itAdd != spellsIn.end(); ++itAdd) {
				spellData->spells[spellIdx] = *itAdd;
				spellIdx += 1;
			}
			_MESSAGE("    0x%08X : %s", raceForm->formID, spellForm->GetFullName());

		}
	}
	
	return true;
}


bool LoadDistribution_WeaponCritTables(const std::string & dataPath)
{
	json distData;
	std::ifstream loadedFile(dataPath.c_str());
	loadedFile >> distData;
	loadedFile.close();
	if (distData.is_null()) {
		_MESSAGE("    ERROR: failed to load distribution file (%s)!", dataPath.c_str());
		return false;
	}

	// weapons crit table enchantments distribution
	if (!distData["weapons"].is_null() && !distData["weapons"].empty()) {
		_MESSAGE("\nDistributing weapon crit table enchantments...");
		json weaponsObj;
		for (json::iterator varWeaponsIt = distData["weapons"].begin(); varWeaponsIt != distData["weapons"].end(); ++varWeaponsIt) {
			weaponsObj = *varWeaponsIt;
			if (weaponsObj.is_null() || weaponsObj.empty()) {
				_MESSAGE("  WARNING: weaponObj is null!");
				continue;
			}
			if (weaponsObj["weapon"].is_null() || weaponsObj["enchantment"].is_null()) {
				_MESSAGE("    WARNING: missing weapon or enchantment form!");
				continue;
			}
			std::string weapID = weaponsObj["weapon"];
			TESObjectWEAP * weapForm = (TESObjectWEAP*)GetFormFromIdentifier(weapID);
			if (!weapForm) {
				_MESSAGE("    WARNING: weapon is null!");
				continue;
			}
			std::string enchID = weaponsObj["enchantment"];
			EnchantmentItem * enchForm = (EnchantmentItem*)GetFormFromIdentifier(enchID);
			if (!enchForm) {
				_MESSAGE("    WARNING: enchantment is null!");
				continue;
			}
			if (!weapForm->weapData.enchantments) {
				weapForm->weapData.enchantments = new tArray<EnchantmentItem*>();
			}
			weapForm->weapData.enchantments->Push(enchForm);
			_MESSAGE("    %s : %s", weapForm->GetFullName(), enchForm->GetFullName());
		}
	}

	// explosions crit table enchantments distribution
	if (!distData["explosions"].is_null() && !distData["explosions"].empty()) {
		_MESSAGE("\nDistributing explosion crit table enchantments...");
		json explosionObj;
		for (json::iterator varExplosionIt = distData["explosions"].begin(); varExplosionIt != distData["explosions"].end(); ++varExplosionIt) {
			explosionObj = *varExplosionIt;
			if (explosionObj.is_null() || explosionObj.empty()) {
				_MESSAGE("    WARNING: explosionObj is null!");
				continue;
			}
			if (explosionObj["explosion"].is_null() || explosionObj["enchantment"].is_null()) {
				_MESSAGE("    WARNING: missing explosion or enchantment form!");
				continue;
			}
			std::string explosionID = explosionObj["explosion"];
			TempBGSExplosion * explosionForm = (TempBGSExplosion*)GetFormFromIdentifier(explosionID);
			if (!explosionForm) {
				_MESSAGE("    WARNING: explosion is null!");
				continue;
			}
			std::string enchID = explosionObj["enchantment"];
			EnchantmentItem * enchForm = (EnchantmentItem*)GetFormFromIdentifier(enchID);
			if (!enchForm) {
				_MESSAGE("    WARNING: enchantment is null!");
				continue;
			}
			explosionForm->objectEffect = enchForm;
			_MESSAGE("    0x%08X : %s", explosionForm->formID, enchForm->GetFullName());
		}
	}

	return true;
}


bool LoadDistribution_ActorCritChances(const std::string & dataPath)
{
	json distData;
	std::ifstream loadedFile(dataPath.c_str());
	loadedFile >> distData;
	loadedFile.close();
	if (distData.is_null()) {
		_MESSAGE("    ERROR: failed to load distribution file (%s)!", dataPath.c_str());
		return false;
	}

	if (!distData["actors"].is_null() && !distData["actors"].empty()) {
		_MESSAGE("\nDistributing actor crit chances...");
		json actorsObj;
		for (json::iterator varActorsIt = distData["actors"].begin(); varActorsIt != distData["actors"].end(); ++varActorsIt) {
			actorsObj = *varActorsIt;
			if (actorsObj.is_null() || actorsObj.empty()) {
				_MESSAGE("    WARNING: actorsObj is null!");
				continue;
			}
			if (actorsObj["actor"].is_null() || actorsObj["critChanceBase"].is_null()) {
				_MESSAGE("    WARNING: missing actor or critChanceBase!");
				continue;
			}
			std::string actorID = actorsObj["actor"];
			TESNPC * actorForm = (TESNPC*)GetFormFromIdentifier(actorID);
			if (!actorForm) {
				_MESSAGE("    WARNING: actor is null!");
				continue;
			}
			float critChance = actorsObj["critChanceBase"];
			if (critChance > 0.0) {
				actorForm->actorValueOwner.SetBase(BCGlobals::avCritChance, critChance);
				_MESSAGE("    0x%08X : %.4f%% crit chance", actorForm->formID, critChance);
			}
		}
	}
	return true;
}


bool BetterCriticals::LoadConfigData()
{
	// -- JSON data:

	json mainConfig;
	std::ifstream loadedFile(".\\Data\\F4SE\\Config\\BetterCriticals\\Config.json");
	loadedFile >> mainConfig;
	loadedFile.close();
	if (mainConfig.is_null()) {
		_MESSAGE("ERROR: Failed to load Config.json!");
		return false;
	}

	// required AVs
	std::string critChanceAVID = mainConfig["avCritChanceID"];
	BCGlobals::avCritChance = (ActorValueInfo*)GetFormFromIdentifier(critChanceAVID);
	if (!BCGlobals::avCritChance) {
		_MESSAGE("ERROR: avCritChance is null!");
		return false;
	}
	std::string critRollModAVID = mainConfig["avCritRollModID"];
	BCGlobals::avCritRollMod = (ActorValueInfo*)GetFormFromIdentifier(critRollModAVID);
	if (!BCGlobals::avCritRollMod) {
		_MESSAGE("ERROR: avCritRollMod is null!");
		return false;
	}
	std::string avCritEffectsTypeID = mainConfig["avCritEffectsTypeID"];
	BCGlobals::avCritEffectsType = (ActorValueInfo*)GetFormFromIdentifier(avCritEffectsTypeID);
	if (!BCGlobals::avCritEffectsType) {
		_MESSAGE("ERROR: avCritEffectsType is null!");
		return false;
	}

	// IsPowerArmorFrame keyword
	BCGlobals::kwIsPowerArmorFrame = (BGSKeyword*)LookupFormByID(0x15503F);
	if (!BCGlobals::kwIsPowerArmorFrame) {
		_MESSAGE("ERROR: kwIsPowerArmorFrame is null! This really shouldn't happen. Either I have failed as a programmer or your game installation is seriously messed up.");
		return false;
	}

	// saving roll avs
	BCGlobals::bCritsUseSavingRoll = mainConfig["bUseSavingRolls"];
	if (BCGlobals::bCritsUseSavingRoll) {
		_MESSAGE("\nLoading saving roll ActorValues index...");
		json savingRollAVsObj;
		for (json::iterator varSavingRollAVsIt = mainConfig["savingRollAVs"].begin(); varSavingRollAVsIt != mainConfig["savingRollAVs"].end(); ++varSavingRollAVsIt) {
			savingRollAVsObj = *varSavingRollAVsIt;
			if (savingRollAVsObj.is_null()) {
				_MESSAGE("    WARNING: savingRollAVs is null!");
				continue;
			}
			ActorValueInfo * savingRollAV = (ActorValueInfo*)GetFormFromIdentifier(savingRollAVsObj);
			if (!savingRollAV) {
				_MESSAGE("    WARNING: savingRollAV is null!");
				continue;
			}
			BCGlobals::specialAVs.push_back(savingRollAV);
			_MESSAGE("    0x%08X - %s", savingRollAV->formID, savingRollAV->avName);
		}
	}
	if (BCGlobals::specialAVs.empty()) {
		_MESSAGE("ERROR: no specialAVs!");
		return false;
	}

	// crit effects index
	_MESSAGE("\nLoading critical effects index...");
	json critEffectsObj;
	for (json::iterator varEffectsIt = mainConfig["critEffects"].begin(); varEffectsIt != mainConfig["critEffects"].end(); ++varEffectsIt) {
		critEffectsObj = *varEffectsIt;
		if (critEffectsObj.is_null()) {
			_MESSAGE("    WARNING: critEffect is null!");
			continue;
		}
		if (critEffectsObj["formID"].is_null()) {
			_MESSAGE("    WARNING: critEffect formID is missing!");
			continue;
		}
		SpellItem * newSpell = (SpellItem*)GetFormFromIdentifier(critEffectsObj["formID"]);
		if (!newSpell) {
			_MESSAGE("    WARNING: newSpell is null!");
			continue;
		}
		BCGlobals::critEffectSpells.push_back(newSpell);
		_MESSAGE("    0x%08X - %s", newSpell->formID, newSpell->GetFullName());
	}
	if (BCGlobals::critEffectSpells.empty()) {
		_MESSAGE("\nERROR: no critEffectSpells!");
		return false;
	}

	// critical effect tables
	std::string sCurFilePath;
	std::vector<std::string> configPathsList;
	std::string sFolderPathStr = ".\\Data\\F4SE\\Config\\BetterCriticals\\CritEffectTables\\";
	configPathsList = GetFileNames(sFolderPathStr);
	if (!configPathsList.empty()) {
		_MESSAGE("\nLoading %i Critical Effect Tables from %s...", configPathsList.size(), sFolderPathStr.c_str());
		for (std::vector<std::string>::iterator itFile = configPathsList.begin(); itFile != configPathsList.end(); ++itFile) {
			sCurFilePath.clear();
			sCurFilePath.append(sFolderPathStr);
			sCurFilePath.append(*itFile);
			CritEffectTable newCritTable = CritEffectTable();
			if (BuildCriticalEffectTableSet(sCurFilePath, newCritTable)) {
				BCGlobals::critEffectTables.push_back(newCritTable);
			}
			else {
				_MESSAGE("\n    ERROR: Failed to create crit effect table from data at %s!", sCurFilePath.c_str());
			}
		}
		configPathsList.clear();
	}
	else {
		_MESSAGE("\nERROR: No Crit Effect Tables found at %s", sFolderPathStr.c_str());
	}
	if (BCGlobals::critEffectTables.empty()) {
		_MESSAGE("\nERROR: final crit effect tables list was empty...");
		return false;
	}

	// distribute crit table set IDs to races
	if (!mainConfig["bDistRaceCritTableIDs"].is_null()) {
		bool distRaceCritTableIDs = mainConfig["bDistRaceCritTableIDs"];
		if (distRaceCritTableIDs) {
			LoadDistribution_RaceCritTableSets(".\\Data\\F4SE\\Config\\BetterCriticals\\Distribution\\RaceCritTableSets.json");
		}
	}
	 // distribute crit table enchantments to weapons
	if (!mainConfig["bDistWeaponCritTables"].is_null()) {
		bool distWeaponCritTables = mainConfig["bDistWeaponCritTables"];
		if (distWeaponCritTables) {
			LoadDistribution_WeaponCritTables(".\\Data\\F4SE\\Config\\BetterCriticals\\Distribution\\WeaponCritTables.json");
		}
	}
	// distribute crit chance to actors
	if (!mainConfig["bDistActorCritChances"].is_null()) {
		bool distNPCCritChances = mainConfig["bDistActorCritChances"];
		if (distNPCCritChances) {
			LoadDistribution_ActorCritChances(".\\Data\\F4SE\\Config\\BetterCriticals\\Distribution\\NPCCritChances.json");
		}
	}

	// Seed the rng
	BCGlobals::rng.Seed();

	_MESSAGE("\nReady.\n\n");
	return true;
}

