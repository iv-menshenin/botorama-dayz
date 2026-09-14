class CfgPatches
{
	class dmBotorama_Loadout
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"dmBotorama_Cons",
			"dmBotorama_Reg",
			"dmBotorama_Core",
		};
	};
};

class CfgMods 
{
	class dmBotorama_Loadout
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_FSM", "DM_WEAPON_DEBUG_FSM", "DM_BOT_DEBUG_SPAWN" };
		class defs 
		{
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_loadout/4_World",
				};
			};
		}; 
	};
};
