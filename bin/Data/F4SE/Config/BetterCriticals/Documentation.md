# Better Criticals 1.0 by isathar



------------------------------------------------------------------------------------
## Config.json

1. avCritChanceID
	- Critical Chance ActorValue
	- Points to Fallout 4's CritChance AV by default

2. avCritRollModID
	- ActorValue to use as the Critical Roll Modifier

3. avCritEffectsTypeID
	- ActorValue to set for races to specify the set of critical effects tables they'll use

3. bUseSavingRolls
	- if enabled, perform a check against an AV on the target to weaken or resist an incoming critical effect

4. savingRollAVs
	- IDs of ActorValues that can be used for saving rolls
	- default: SPECIAL

5. critEffects
	- Spell index for critical effect tables
	- Spells should be set up as Instant Cast - Fire and Forget - Target Actor
	- formID is required - info is not used
	- Critical effect spellIds are the effects indices in this list

6. races
	- Associates races with their critical effect tables
	- formID and critTableID are required - info is not used
	- 0 to disable crit effects for a race (does not need to be specified, just omit the race from the list)
	- optional (can also be done using plugins)

7. weaponBaseCritTypes
	- Associates weapons with their base critical effect enchantments
	- weapon = weapon formID
	- enchantment = crit effect enchantment formID
	- optional (can also be done using plugins)

------------------------------------------------------------------------------------
## CritEffectTables folder

1. name *(String - optional)*
	- crit table name used for organization and log output

2. targetTypeID *(FormID String - required)*
	- actor keyword required to use this set of effect tables

3. critTables *(list - required)*
	- iCritTypeID *(int - required)*
		- index of the DamageType that triggers effects from this table
	- sImmuneKW *(FormID String - optional)*
		- actor keyword that specifies if a target is immune to this effect type
	- effects *(list - required)*
		- iRollMax *(int - required)* 
			- the maximum roll value for this crit effect - should be < 256
		- sSpellID *(FormID String - required)* 
			- the critical effect spell to apply (or none)
		- sSpellIDSaved *(FormID String - optional)* 
			- the critical spell to apply if the target passes its saving roll (or none)
		- iSavingRollAV *(int - optional)* 
			- the index of the SPECIAL attribute to use in the saving roll (or -1 for none)
		- iSavingRollMod *(int - optional)* 
			- the modifier to use for the saving roll (positive=easier, negative=harder)

------------------------------------------------------------------------------------

## Distribution folder

